#pragma once

#include <atomic>
#include <thread>
#include <cstring>
#include <chrono>

// Platform-specific socket includes
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
    #include <fcntl.h>
    #define SOCKET int
    #define CLOSE_SOCKET close
    #define SOCKET_ERROR_VAL -1
#endif


namespace nerve {


// ─────────────────────────────────────────────────────────
// Face Data — shared between UDP thread and audio thread
// ─────────────────────────────────────────────────────────

struct FaceData {
    // v1 fields (17 floats)
    float headX   = 0.f;
    float headY   = 0.f;
    float headZ   = 0.f;
    float headDist = 0.f;
    float leftEye  = 0.f;
    float rightEye = 0.f;
    float gazeX = 0.f;
    float gazeY = 0.f;
    float mouthW = 0.f;
    float mouthH = 0.f;
    float jaw    = 0.f;
    float lips   = 0.f;
    float browL = 0.f;
    float browR = 0.f;
    float blinkL = 0.f;
    float blinkR = 0.f;
    float expression = 0.f;
    // v2 fields (4 additional floats)
    float tongue = 0.f;
    float browInnerUp = 0.f;
    float browDownL = 0.f;
    float browDownR = 0.f;

    uint64_t timestamp = 0;
    int faceCount = 0;
    bool valid = false;
};


// ─────────────────────────────────────────────────────────
// Double-Buffer — lock-free latest-state communication
// ─────────────────────────────────────────────────────────

struct FaceDataBuffer {
    FaceData buffers[2];
    std::atomic<int> active{0};
    std::atomic<uint64_t> version{0};

    void write(const FaceData& data) {
        int inactive = 1 - active.load(std::memory_order_relaxed);
        buffers[inactive] = data;
        active.store(inactive, std::memory_order_release);
        version.fetch_add(1, std::memory_order_release);
    }

    const FaceData& read() const {
        int idx = active.load(std::memory_order_acquire);
        return buffers[idx];
    }

    uint64_t getVersion() const {
        return version.load(std::memory_order_acquire);
    }
};


// ─────────────────────────────────────────────────────────
// UDP Packet Parser
// ─────────────────────────────────────────────────────────

static const char NERVE_MAGIC[4] = {'N', 'E', 'R', 'V'};
static const int NERVE_HEADER_SIZE = 8;
static const int NERVE_V1_PACKET_SIZE = 84;   // 17 floats
static const int NERVE_V2_PACKET_SIZE = 100;  // 21 floats

inline bool parsePacket(const char* buf, int len, FaceData& out) {
    if (len < NERVE_V1_PACKET_SIZE) return false;
    if (std::memcmp(buf, NERVE_MAGIC, 4) != 0) return false;

    uint16_t version;
    std::memcpy(&version, buf + 4, 2);
    if (version < 1 || version > 2) return false;

    uint16_t faceCount;
    std::memcpy(&faceCount, buf + 6, 2);
    if (faceCount < 1 || faceCount > 4) return false;

    const char* faceBlock = buf + NERVE_HEADER_SIZE;

    auto readFloat = [&](int offset) -> float {
        float val;
        std::memcpy(&val, faceBlock + offset, 4);
        return val;
    };

    // v1 fields (17 floats, offsets 0-67)
    out.headX      = rack::math::clamp(readFloat(0),  -1.f, 1.f);
    out.headY      = rack::math::clamp(readFloat(4),  -1.f, 1.f);
    out.headZ      = rack::math::clamp(readFloat(8),  -1.f, 1.f);
    out.headDist   = rack::math::clamp(readFloat(12),  0.f, 1.f);
    out.leftEye    = rack::math::clamp(readFloat(16),  0.f, 1.f);
    out.rightEye   = rack::math::clamp(readFloat(20),  0.f, 1.f);
    out.gazeX      = rack::math::clamp(readFloat(24), -1.f, 1.f);
    out.gazeY      = rack::math::clamp(readFloat(28), -1.f, 1.f);
    out.mouthW     = rack::math::clamp(readFloat(32),  0.f, 1.f);
    out.mouthH     = rack::math::clamp(readFloat(36),  0.f, 1.f);
    out.jaw        = rack::math::clamp(readFloat(40),  0.f, 1.f);
    out.lips       = rack::math::clamp(readFloat(44),  0.f, 1.f);
    out.browL      = rack::math::clamp(readFloat(48),  0.f, 1.f);
    out.browR      = rack::math::clamp(readFloat(52),  0.f, 1.f);
    out.blinkL     = readFloat(56) > 0.5f ? 1.f : 0.f;
    out.blinkR     = readFloat(60) > 0.5f ? 1.f : 0.f;
    out.expression = rack::math::clamp(readFloat(64),  0.f, 1.f);

    // v2 fields (4 additional floats, offsets 68-83)
    if (version >= 2 && len >= NERVE_V2_PACKET_SIZE) {
        out.tongue     = rack::math::clamp(readFloat(68),  0.f, 1.f);
        out.browInnerUp = rack::math::clamp(readFloat(72), 0.f, 1.f);
        out.browDownL  = rack::math::clamp(readFloat(76),  0.f, 1.f);
        out.browDownR  = rack::math::clamp(readFloat(80),  0.f, 1.f);
        std::memcpy(&out.timestamp, faceBlock + 84, 8);
    } else {
        out.tongue = 0.f;
        out.browInnerUp = 0.f;
        out.browDownL = 0.f;
        out.browDownR = 0.f;
        std::memcpy(&out.timestamp, faceBlock + 68, 8);
    }

    out.faceCount = faceCount;
    out.valid = true;

    return true;
}


// ─────────────────────────────────────────────────────────
// UDP Listener Thread
// ─────────────────────────────────────────────────────────

class UDPListener {
public:
    UDPListener(FaceDataBuffer* buffer) : faceBuffer(buffer) {}
    ~UDPListener() { stop(); }

    void start(int port = 9000) {
        if (running.load()) return;
        listenPort = port;
        shouldStop.store(false);
        running.store(true);
        listenerThread = std::thread(&UDPListener::run, this);
    }

    void stop() {
        shouldStop.store(true);
        if (listenerThread.joinable()) {
            listenerThread.join();
        }
        running.store(false);
    }

    bool isRunning() const { return running.load(); }
    int getPort() const { return listenPort; }
    std::atomic<float> currentFPS{0.f};

private:
    void run() {
        #ifdef ARCH_WIN
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
        #endif

        SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == SOCKET_ERROR_VAL) {
            running.store(false);
            return;
        }

        int reuse = 1;
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

        #ifdef ARCH_WIN
        DWORD timeout = 100;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        #else
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        #endif

        struct sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(listenPort);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            CLOSE_SOCKET(sock);
            running.store(false);
            return;
        }

        int frameCount = 0;
        auto fpsTimer = std::chrono::steady_clock::now();

        char buf[512];
        while (!shouldStop.load()) {
            int n = recvfrom(sock, buf, sizeof(buf), 0, nullptr, nullptr);
            if (n <= 0) continue;

            FaceData newData;
            if (parsePacket(buf, n, newData)) {
                if (newData.timestamp == 0) {
                    newData.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now().time_since_epoch()
                    ).count();
                }
                faceBuffer->write(newData);
                frameCount++;
            }

            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - fpsTimer).count();
            if (elapsed >= 1.f) {
                currentFPS.store(frameCount / elapsed);
                frameCount = 0;
                fpsTimer = now;
            }
        }

        CLOSE_SOCKET(sock);

        #ifdef ARCH_WIN
        WSACleanup();
        #endif

        running.store(false);
    }

    FaceDataBuffer* faceBuffer;
    std::thread listenerThread;
    std::atomic<bool> shouldStop{false};
    std::atomic<bool> running{false};
    int listenPort = 9000;
};


}  // namespace nerve
