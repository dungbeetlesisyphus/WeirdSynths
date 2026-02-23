#pragma once

// DepthUDP.hpp — Kinect Bridge protocol receiver for DEPTH module
// Listens for KINT packets on UDP port 9005 (depth CVs)
// and SKEL packets on UDP port 9006 (skeleton joints).
//
// KINT Packet v1 — 48 bytes
//   [0-3]   Magic "KINT"
//   [4-5]   Version uint16 LE
//   [6]     Source  uint8  (0=K360, 1=KOne, 2=Azure, 3=Sim)
//   [7]     BodyCount uint8
//   [8-39]  8 × float32 LE  (depth CV values)
//   [40-47] Timestamp uint64 LE
//
// CV layout (matches kinect_bridge.py):
//   float[0]  DIST    nearest foreground, 0-1
//   float[1]  MOTION  frame diff energy,  0-1
//   float[2]  CNTX    horizontal centroid, -1..+1
//   float[3]  CNTY    vertical centroid,   -1..+1
//   float[4]  AREA    foreground fraction, 0-1
//   float[5]  DPTHL   left zone depth,     0-1
//   float[6]  DPTHR   right zone depth,    0-1
//   float[7]  ENTR    entropy/complexity,  0-1
//
// SKEL Packet v1 — variable
//   [0-3]   Magic "SKEL"
//   [4-5]   Version uint16 LE
//   [6]     BodyIndex uint8
//   [7]     JointCount uint8
//   [8-N]   JointCount × 3 × float32 (x,y,z each -1..+1)
//   [N+8]   Timestamp uint64 LE

#include <atomic>
#include <thread>
#include <cstring>
#include <chrono>
#include <algorithm>

#ifdef ARCH_WIN
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
    #define CLOSE_SOCKET closesocket
    #define SOCKET_ERROR_VAL INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_VAL -1
#endif


namespace depth {


// ─────────────────────────────────────────────────────────
// Data Structs
// ─────────────────────────────────────────────────────────

enum class KinectSource : uint8_t {
    K360      = 0,
    KOne      = 1,
    Azure     = 2,
    Simulated = 3,
    Unknown   = 255
};

static const char* kinectSourceName(KinectSource s) {
    switch (s) {
        case KinectSource::K360:      return "Kinect 360";
        case KinectSource::KOne:      return "Kinect One";
        case KinectSource::Azure:     return "Azure Kinect";
        case KinectSource::Simulated: return "Simulated";
        default:                      return "Unknown";
    }
}

// 8 normalized CV outputs from depth field analysis
struct DepthCVs {
    float dist    = 0.f;  // nearest foreground depth 0-1 (1=very close)
    float motion  = 0.f;  // frame-to-frame motion energy 0-1
    float cntX    = 0.f;  // horizontal centroid -1..+1
    float cntY    = 0.f;  // vertical centroid   -1..+1
    float area    = 0.f;  // foreground fraction 0-1
    float depthL  = 0.f;  // left zone mean depth 0-1
    float depthR  = 0.f;  // right zone mean depth 0-1
    float entropy = 0.f;  // depth field complexity 0-1
};

struct DepthData {
    DepthCVs cvs;
    KinectSource source    = KinectSource::Unknown;
    uint8_t  bodyCount     = 0;
    uint64_t timestamp     = 0;
    bool     valid         = false;
};

// Up to 2 bodies, 32 joints each, (x,y,z) normalized -1..+1
static const int MAX_SKEL_BODIES = 2;
static const int MAX_SKEL_JOINTS = 32;

struct JointXYZ { float x, y, z; };

struct SkeletonBody {
    uint8_t  bodyIndex  = 0;
    uint8_t  jointCount = 0;
    JointXYZ joints[MAX_SKEL_JOINTS] = {};
    bool     valid = false;
};

struct SkeletonData {
    SkeletonBody bodies[MAX_SKEL_BODIES];
    uint8_t  bodyCount = 0;
    uint64_t timestamp = 0;
};


// ─────────────────────────────────────────────────────────
// Double-Buffer — lock-free latest-state handoff
// ─────────────────────────────────────────────────────────

template<typename T>
struct DoubleBuffer {
    T buffers[2];
    std::atomic<int> active{0};
    std::atomic<uint64_t> version{0};

    void write(const T& data) {
        int inactive = 1 - active.load(std::memory_order_relaxed);
        buffers[inactive] = data;
        active.store(inactive, std::memory_order_release);
        version.fetch_add(1, std::memory_order_release);
    }

    const T& read() const {
        return buffers[active.load(std::memory_order_acquire)];
    }

    uint64_t getVersion() const {
        return version.load(std::memory_order_acquire);
    }
};

using DepthDataBuffer    = DoubleBuffer<DepthData>;
using SkeletonDataBuffer = DoubleBuffer<SkeletonData>;


// ─────────────────────────────────────────────────────────
// Packet Parsers
// ─────────────────────────────────────────────────────────

static const char KINT_MAGIC[4] = {'K', 'I', 'N', 'T'};
static const char SKEL_MAGIC[4] = {'S', 'K', 'E', 'L'};
static const int  KINT_PACKET_SIZE = 48;
static const int  SKEL_MIN_SIZE    = 16;  // header + footer, no joints

inline bool parseKintPacket(const char* buf, int len, DepthData& out) {
    if (len < KINT_PACKET_SIZE) return false;
    if (std::memcmp(buf, KINT_MAGIC, 4) != 0) return false;

    uint16_t version;
    std::memcpy(&version, buf + 4, 2);
    if (version != 1) return false;

    out.source    = static_cast<KinectSource>(static_cast<uint8_t>(buf[6]));
    out.bodyCount = static_cast<uint8_t>(buf[7]);

    auto rf = [&](int offset) -> float {
        float v; std::memcpy(&v, buf + 8 + offset, 4); return v;
    };

    using rack::math::clamp;
    out.cvs.dist    = clamp(rf( 0), 0.f, 1.f);
    out.cvs.motion  = clamp(rf( 4), 0.f, 1.f);
    out.cvs.cntX    = clamp(rf( 8), -1.f, 1.f);
    out.cvs.cntY    = clamp(rf(12), -1.f, 1.f);
    out.cvs.area    = clamp(rf(16), 0.f, 1.f);
    out.cvs.depthL  = clamp(rf(20), 0.f, 1.f);
    out.cvs.depthR  = clamp(rf(24), 0.f, 1.f);
    out.cvs.entropy = clamp(rf(28), 0.f, 1.f);

    std::memcpy(&out.timestamp, buf + 40, 8);
    out.valid = true;
    return true;
}

inline bool parseSkelPacket(const char* buf, int len, SkeletonBody& out) {
    if (len < SKEL_MIN_SIZE) return false;
    if (std::memcmp(buf, SKEL_MAGIC, 4) != 0) return false;

    uint16_t version;
    std::memcpy(&version, buf + 4, 2);
    if (version != 1) return false;

    out.bodyIndex  = static_cast<uint8_t>(buf[6]);
    out.jointCount = static_cast<uint8_t>(buf[7]);

    // Clamp to our array size
    int jc = std::min<int>(out.jointCount, MAX_SKEL_JOINTS);
    int expected = 8 + jc * 12 + 8;   // header + joints + timestamp
    if (len < expected) return false;

    for (int i = 0; i < jc; i++) {
        const char* base = buf + 8 + i * 12;
        std::memcpy(&out.joints[i].x, base + 0, 4);
        std::memcpy(&out.joints[i].y, base + 4, 4);
        std::memcpy(&out.joints[i].z, base + 8, 4);
        using rack::math::clamp;
        out.joints[i].x = clamp(out.joints[i].x, -1.f, 1.f);
        out.joints[i].y = clamp(out.joints[i].y, -1.f, 1.f);
        out.joints[i].z = clamp(out.joints[i].z, -1.f, 1.f);
    }

    out.valid = true;
    return true;
}


// ─────────────────────────────────────────────────────────
// UDP Listener — listens on one port, dispatches two packet types
// ─────────────────────────────────────────────────────────

class DepthUDPListener {
public:
    DepthUDPListener(DepthDataBuffer* db, SkeletonDataBuffer* sb)
        : depthBuf(db), skelBuf(sb) {}

    ~DepthUDPListener() { stop(); }

    void start(int depthPort = 9005, int skelPort = 9006) {
        if (running.load()) return;
        this->depthListenPort = depthPort;
        this->skelListenPort  = skelPort;
        shouldStop.store(false);
        running.store(true);
        depthThread = std::thread(&DepthUDPListener::runDepth, this);
        skelThread  = std::thread(&DepthUDPListener::runSkel,  this);
    }

    void stop() {
        shouldStop.store(true);
        if (depthThread.joinable()) depthThread.join();
        if (skelThread.joinable())  skelThread.join();
        running.store(false);
    }

    bool isRunning() const { return running.load(); }

    std::atomic<float> depthFPS{0.f};
    std::atomic<int>   lastBodyCount{0};

private:

    SOCKET openUDP(int port) {
        #ifdef ARCH_WIN
        WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData);
        #endif

        SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s == SOCKET_ERROR_VAL) return SOCKET_ERROR_VAL;

        int reuse = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        #ifdef ARCH_WIN
        DWORD timeout = 100;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        #else
        struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 100000;
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        #endif

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            CLOSE_SOCKET(s);
            return SOCKET_ERROR_VAL;
        }
        return s;
    }

    void runDepth() {
        SOCKET s = openUDP(depthListenPort);
        if (s == SOCKET_ERROR_VAL) { running.store(false); return; }

        int fc = 0;
        auto timer = std::chrono::steady_clock::now();
        char buf[512];

        while (!shouldStop.load()) {
            int n = recvfrom(s, buf, sizeof(buf), 0, nullptr, nullptr);
            if (n <= 0) continue;

            DepthData d;
            if (parseKintPacket(buf, n, d)) {
                if (d.timestamp == 0) {
                    d.timestamp = (uint64_t)std::chrono::duration_cast<
                        std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()).count();
                }
                depthBuf->write(d);
                lastBodyCount.store(d.bodyCount);
                fc++;
            }

            auto now = std::chrono::steady_clock::now();
            float el = std::chrono::duration<float>(now - timer).count();
            if (el >= 1.f) {
                depthFPS.store(fc / el);
                fc = 0; timer = now;
            }
        }
        CLOSE_SOCKET(s);
    }

    void runSkel() {
        SOCKET s = openUDP(skelListenPort);
        if (s == SOCKET_ERROR_VAL) return;

        char buf[2048];   // large enough for 32 joints × 12 bytes + header/footer

        while (!shouldStop.load()) {
            int n = recvfrom(s, buf, sizeof(buf), 0, nullptr, nullptr);
            if (n <= 0) continue;

            SkeletonBody body;
            if (parseSkelPacket(buf, n, body)) {
                // Merge into skeleton data buffer
                const SkeletonData& cur = skelBuf->read();
                SkeletonData updated = cur;
                int idx = std::min<int>(body.bodyIndex, MAX_SKEL_BODIES - 1);
                updated.bodies[idx] = body;
                updated.bodyCount = std::max<uint8_t>(updated.bodyCount, idx + 1);
                updated.timestamp = (uint64_t)std::chrono::duration_cast<
                    std::chrono::microseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count();
                skelBuf->write(updated);
            }
        }
        CLOSE_SOCKET(s);
    }

    DepthDataBuffer*    depthBuf;
    SkeletonDataBuffer* skelBuf;
    std::thread         depthThread;
    std::thread         skelThread;
    std::atomic<bool>   shouldStop{false};
    std::atomic<bool>   running{false};
    int depthListenPort = 9005;
    int skelListenPort  = 9006;
};


// ─────────────────────────────────────────────────────────
// Slew-Rate Limiter — smooth CV transitions
// ─────────────────────────────────────────────────────────

struct SlewLimiter {
    float current = 0.f;

    // alpha: 0 = no slew (immediate), 1 = infinite slew (never moves)
    // Typical: 0.85-0.98
    inline float process(float target, float alpha) {
        current = alpha * current + (1.f - alpha) * target;
        return current;
    }

    inline void reset(float val = 0.f) { current = val; }
};


}  // namespace depth
