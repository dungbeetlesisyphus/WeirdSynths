# WeirdConnect Protocol v3 — ARKit Profile

Complete binary protocol specification for real-time biometric CV streaming from WeirdPhone iOS app to VCV Rack NERVE module, DAWs, and custom audio software.

---

## Overview

WeirdConnect v3 is a lightweight UDP-based protocol optimized for low-latency transmission of ARKit face tracking data. Each packet contains 64 continuous control values (52 blendshapes + 6 head pose + 4 eye gaze) at up to 60 FPS.

**Key Properties**
- **Transport**: UDP unicast or broadcast
- **Packet Size**: 260 bytes (fixed)
- **Frame Rate**: Configurable 10–60 FPS (default 60)
- **Latency Budget**: <8ms end-to-end (typical 3–7ms)
- **Bandwidth**: ~156 KB/s @ 60 FPS
- **Byte Order**: Little-endian (Intel x86/ARM standard)
- **Compatibility**: NERVE v2 backward compatible, VCV Rack 2.0+

---

## Packet Format (Binary)

All multi-byte values are encoded as little-endian. All floating-point values use IEEE 754 single precision (float32).

### Packet Structure

```
Offset  Size   Type      Field                    Description
─────────────────────────────────────────────────────────────────
0       4      uint8[4]  Magic Header             0x57 0x52 0x44 0x03 ("WRD\x03")
4       4      uint32    Sequence Number         Packet counter (monotonic, wraps at 2^32)
8       4      float32   Timestamp               Seconds since app session start
12      4      float32   Blendshape 0            browDownLeft [0.0–1.0]
16      4      float32   Blendshape 1            browDownRight [0.0–1.0]
20      4      float32   Blendshape 2            browInnerUp [0.0–1.0]
...     ...    ...       ...                     (Blendshapes 3–50, same format)
208     4      float32   Blendshape 51           tongueOut [0.0–1.0]
212     4      float32   Head Pitch              Radians, -π/2 to +π/2
216     4      float32   Head Yaw                Radians, -π to +π
220     4      float32   Head Roll               Radians, -π/2 to +π/2
224     4      float32   Head Tx                 Normalized, -1.0 to +1.0
228     4      float32   Head Ty                 Normalized, -1.0 to +1.0
232     4      float32   Head Tz                 Normalized, -1.0 to +1.0
236     4      float32   Eye Gaze Left X         Direction, -1.0 to +1.0
240     4      float32   Eye Gaze Left Y         Direction, -1.0 to +1.0
244     4      float32   Eye Gaze Right X        Direction, -1.0 to +1.0
248     4      float32   Eye Gaze Right Y        Direction, -1.0 to +1.0
252     4      uint32    Reserved                All zeros (future use)
256     4      uint32    Checksum (CRC32)        Optional, for reliability
260     (END)
```

**Total Packet Size**: 260 bytes

### Magic Header

**Bytes 0–3**: `0x57 0x52 0x44 0x03`

- **0x57**: 'W' (ASCII)
- **0x52**: 'R' (ASCII)
- **0x44**: 'D' (ASCII)
- **0x03**: Version 3 (uint8)

**Purpose**: Identify WeirdConnect packets and protocol version. Receivers check the first 3 bytes for "WRD", then compare the 4th byte to version.

**Version History**
- `0x02`: WeirdConnect v2 (legacy, 8-value format)
- `0x03`: WeirdConnect v3 (ARKit profile, 64 values)
- Future: `0x04`, `0x05`, etc.

### Sequence Number

**Bytes 4–7**: `uint32` (little-endian)

Monotonically incrementing packet counter starting at 0. Wraps at 2^32 (4,294,967,295). Receiver can detect packet loss by monitoring gaps.

**Example**
- Packet 1: seq = 0
- Packet 2: seq = 1
- ...
- Packet N: seq = N-1
- Gap detected if seq jumps more than expected (e.g., seq 100 then 105)

### Timestamp

**Bytes 8–11**: `float32` (IEEE 754)

Seconds elapsed since WeirdPhone app started tracking. Zero at app launch or after calibration. Monotonically increasing (may have small jumps due to system clock adjustments).

**Range**: 0.0 to ~86,400 seconds (1 day of continuous operation before rollover)

**Resolution**: ~1 millisecond (sufficient for 60 FPS @ 16.67ms per frame)

### Blendshape Values (Bytes 12–207)

**52 consecutive float32 values**, one per ARKit blendshape in canonical order.

Each value is normalized to **[0.0, 1.0]**:
- 0.0 = Fully inactive (no blendshape movement)
- 0.5 = Medium activation
- 1.0 = Full activation (maximum extent)

#### Blendshape Canonical Order (52 total)

| Index | Name | Index | Name | Index | Name | Index | Name |
|-------|------|-------|------|-------|------|-------|------|
| 0 | browDownLeft | 13 | eyeBlinkRight | 26 | jawForward | 39 | mouthSmileLeft |
| 1 | browDownRight | 14 | eyeLookDownLeft | 27 | jawLeft | 40 | mouthSmileRight |
| 2 | browInnerUp | 15 | eyeLookDownRight | 28 | jawRight | 41 | mouthFrownLeft |
| 3 | browOuterUpLeft | 16 | eyeLookInLeft | 29 | jawOpen | 42 | mouthFrownRight |
| 4 | browOuterUpRight | 17 | eyeLookInRight | 30 | mouthClose | 43 | mouthDimpleLeft |
| 5 | cheekPuff | 18 | eyeLookOutLeft | 31 | mouthFunnel | 44 | mouthDimpleRight |
| 6 | cheekSquintLeft | 19 | eyeLookOutRight | 32 | mouthPucker | 45 | mouthUpperLipUp |
| 7 | cheekSquintRight | 20 | eyeLookUpLeft | 33 | mouthLeft | 46 | mouthLowerLipDown |
| 8 | cheekRaiseLeft | 21 | eyeLookUpRight | 34 | mouthRight | 47 | mouthUpperLipSuckUpper |
| 9 | cheekRaiseRight | 22 | eyeSquintLeft | 35 | mouthRollLower | 48 | mouthLowerLipSuckLower |
| 10 | chinRaise | 23 | eyeSquintRight | 36 | mouthRollUpper | 49 | mouthUpperLipPress |
| 11 | chinLowerDown | 24 | eyeWideLeft | 37 | mouthShrugLower | 50 | mouthLowerLipPress |
| 12 | eyeBlinkLeft | 25 | eyeWideRight | 38 | mouthShrugUpper | 51 | tongueOut |

**Byte Layout**:
- Blendshape 0 (browDownLeft): bytes 12–15
- Blendshape 1 (browDownRight): bytes 16–19
- ...
- Blendshape 51 (tongueOut): bytes 204–207

### Head Pose Values (Bytes 212–235)

6 float32 values representing 6 degrees of freedom:

| Offset | Field | Unit | Range | Description |
|--------|-------|------|-------|-------------|
| 212–215 | Pitch | radians | -π/2 to +π/2 | Head tilt (nod forward/backward) |
| 216–219 | Yaw | radians | -π to +π | Head turn (shake left/right) |
| 220–223 | Roll | radians | -π/2 to +π/2 | Head rotation (tilt left/right) |
| 224–227 | Tx | normalized | -1.0 to +1.0 | Head X position (left/right) |
| 228–231 | Ty | normalized | -1.0 to +1.0 | Head Y position (up/down) |
| 232–235 | Tz | normalized | -1.0 to +1.0 | Head Z position (toward/away) |

**Coordinate System**
- **Pitch**: Euler angle, positive = head tilted back (looking up)
- **Yaw**: Euler angle, positive = head rotated right
- **Roll**: Euler angle, positive = head tilted right
- **Tx/Ty/Tz**: Normalized position relative to camera frame center

### Eye Gaze Values (Bytes 236–251)

4 float32 values representing independent left/right eye gaze direction:

| Offset | Field | Range | Description |
|--------|-------|-------|-------------|
| 236–239 | Left Gaze X | -1.0 to +1.0 | Left eye horizontal direction |
| 240–243 | Left Gaze Y | -1.0 to +1.0 | Left eye vertical direction |
| 244–247 | Right Gaze X | -1.0 to +1.0 | Right eye horizontal direction |
| 248–251 | Right Gaze Y | -1.0 to +1.0 | Right eye vertical direction |

**Gaze Vector Interpretation**
- X component: -1.0 (far left) to +1.0 (far right)
- Y component: -1.0 (far down) to +1.0 (far up)
- Magnitude: 0.0–1.4 (normalized direction, not unit vectors)

**Example**: User looking straight ahead → gazeX ≈ 0, gazeY ≈ 0. Looking left → gazeX ≈ -0.8.

### Reserved & Checksum (Bytes 252–259)

**Bytes 252–255**: Reserved (uint32, all zeros)
- Reserved for future protocol extensions
- Receivers must ignore and treat as zeros
- Senders must write as 0x00 0x00 0x00 0x00

**Bytes 256–259**: CRC32 Checksum (uint32, little-endian)
- Optional error detection (default: 0x00000000 if unused)
- CRC32 polynomial: MPEG-2 (0x04C11DB7)
- Computed over bytes 0–255 (entire packet except this field)
- Receiver should verify before processing (recommended for reliability)

---

## Protocol Variants

### UDP Transport Modes

#### Unicast
- Sender: WeirdPhone sends to single IP:port
- Receiver: Single target computer
- Typical setup: `192.168.1.100:9000` (NERVE module on fixed machine)

#### Broadcast
- Sender: WeirdPhone sends to `255.255.255.255:9000` (broadcast address)
- Receiver: Any device listening on the network segment
- Useful for discovery; enables multiple simultaneous receivers
- Requires WeirdPhone "Broadcast Mode" enabled (Settings → Network)

#### Multicast (Future)
- Not implemented in v3.0, but reserved for v4.0+
- Would allow selective subscription to device groups

---

## NERVE Module Compatibility

### NERVE v3 (New)

**Auto-Detection**
1. Module listens on UDP port 9000 (hardcoded or configurable)
2. When packet arrives, reads first 4 bytes
3. Checks magic: if `0x57 0x52 0x44` ("WRD"), continues
4. Reads version byte (4th byte):
   - If `0x03` → Parse as WeirdConnect v3 (64 values)
   - If `0x02` → Parse as WeirdConnect v2 (legacy, 8 values)
   - Otherwise: Log warning, ignore packet

**NERVE v3 Output Mapping**

8 standard CV outputs (free tier):

| Output | Blendshape | Notes |
|--------|-----------|-------|
| CV Out 0 | jawOpen | Gate/amplitude envelope |
| CV Out 1 | headPitch | Pitch modulation (scaled ±π → ±5V) |
| CV Out 2 | headYaw | Pan modulation |
| CV Out 3 | headRoll | Rotation effect |
| CV Out 4 | eyeBlinkLeft | Gate trigger |
| CV Out 5 | eyeBlinkRight | Gate trigger |
| CV Out 6 | mouthSmileLeft | Sentiment/brightness |
| CV Out 7 | mouthSmileRight | Sentiment/brightness |

Scaling Formula:
```
CV_voltage = (blendshape_value - 0.5) * 10.0  // Range: -5V to +5V
CV_voltage = blendshape_value * 10.0          // Range: 0V to +10V (common alternative)
```

**NERVE v3 Context Menu (Right-click)**

- "Connect to WeirdPhone" → Modal dialog showing:
  - Discovered WeirdPhone devices on network (via broadcast scan on 9001)
  - Manual IP/port entry field
  - "Connect" button (saves to module config)
  - "Rescan" button (refreshes device list)

**NERVE v3 Module Display**

Module title bar shows:
```
WeirdConnect v3 — iPhone [device_name] @ [IP]:[port]
```

Status indicator (LED-style):
- Green: Connected, receiving packets
- Yellow: Searching / retrying connection
- Red: Connection lost or error

### NERVE v2 (Legacy)

**Reading v3 Packets**
- NERVE v2 reads only first 8 CV values from packet
- Ignores bytes 208+ (head pose, eye gaze, extra data)
- Packet magic check: If "WRD\x02", use v2 format. If "WRD\x03", fall back to reading first 8 blendshapes.
- Result: Seamless backward compatibility

**Recommendation**
- Users on NERVE v2 can use WeirdPhone v0.1 with reduced control set (8 values)
- Upgrade to NERVE v3 module to unlock full 64-value set

### NERVE Pro Expansion (Future)

**Planned NERVE Pro Module**
- 16 CV outputs (8 additional)
- Configurable mapping (MAP screen integration)
- Preset sync with WeirdPhone app
- Advanced curve/smoothing controls

---

## Discovery Protocol

### WeirdPhone Discovery Broadcast

Enables NERVE and other software to auto-discover WeirdPhone devices on the network.

**Discovery Mechanism**
1. WeirdPhone broadcasts on port 9001 every 5 seconds (if "Broadcast Mode" enabled)
2. Broadcast address: `255.255.255.255` (subnet broadcast)
3. Payload: Plain ASCII text (null-terminated)

**Broadcast Message Format**
```
WEIRDPHONE:v3:[device_name]:[local_ip]:[port]
```

**Example Payloads**
```
WEIRDPHONE:v3:iPhone 15 Pro:192.168.1.105:9000
WEIRDPHONE:v3:Alice's iPhone:10.0.0.42:9000
WEIRDPHONE:v3:Studio Device:192.168.100.50:9000
```

**Broadcast Details**
- **Port**: 9001 (fixed)
- **Interval**: 5 seconds (default, configurable in Settings)
- **Format**: ASCII plaintext, newline-terminated
- **Max Length**: 256 bytes

### NERVE Discovery Handler

**NERVE Listening Loop** (pseudo-code)
```
while (listening) {
  packet = receive_udp(port: 9001, timeout: 30s)
  if (packet.startsWith("WEIRDPHONE:v3:")) {
    parts = packet.split(":")
    device_name = parts[2]
    device_ip = parts[3]
    device_port = parts[4]
    discovered_devices.append({device_name, device_ip, device_port})
  }
}
```

**Discovery UI** (in NERVE context menu)
- List all discovered devices
- Click to auto-fill IP/port in module settings
- Devices timeout after 30s without refresh

---

## Latency Budget Breakdown

Target: **<8ms glass-to-CV** (from face visible to ARKit, to network, to module output)

### Latency Components

| Stage | Duration | Notes |
|-------|----------|-------|
| **ARKit Capture** | ~4ms | TrueDepth camera frame time |
| **Face Detection** | ~0.5ms | Neural Engine (A-series, hardware) |
| **Blendshape Calc** | ~2ms | ARKit pipeline |
| **App Serialize** | ~0.2ms | Swift struct → bytes |
| **UDP Send** | ~0.1ms | System network stack |
| **Network Transit** | 1–3ms | WiFi propagation (1–10m typical) |
| **Module Receive** | ~0.2ms | UDP socket read |
| **Module Parse** | ~0.02ms | C++ struct deserialize |
| **Module Output** | ~0.02ms | Update CV voltage |
| **Total** | **~7–10ms** | Typical: 6–8ms |

**Optimization Strategies**
- Use UDP (lower overhead than TCP)
- Broadcast mode reduces handshakes
- Minimize WiFi interference (5GHz preferred, less crowded)
- Keep app foreground (no iOS background throttling)
- Set NERVE module to low latency mode (if available)

---

## Implementation Reference

### Pseudo-code: Packet Writing (iOS Swift)

```swift
func serializePacket(_ frame: FaceFrameData) -> Data {
    var packet = Data(capacity: 260)
    
    // Magic header
    packet.append(contentsOf: [0x57, 0x52, 0x44, 0x03])
    
    // Sequence number (little-endian uint32)
    packet.append(sequenceNumber.littleEndianBytes)
    
    // Timestamp (float32)
    packet.append(Float(frame.timestamp).littleEndianBytes)
    
    // 52 blendshapes (float32 each)
    for blendshape in frame.blendshapes {
        packet.append(blendshape.littleEndianBytes)
    }
    
    // Head pose (6 floats)
    packet.append(frame.headPose.pitch.littleEndianBytes)
    packet.append(frame.headPose.yaw.littleEndianBytes)
    packet.append(frame.headPose.roll.littleEndianBytes)
    packet.append(frame.headPose.tx.littleEndianBytes)
    packet.append(frame.headPose.ty.littleEndianBytes)
    packet.append(frame.headPose.tz.littleEndianBytes)
    
    // Eye gaze (4 floats)
    packet.append(frame.eyeGaze.leftX.littleEndianBytes)
    packet.append(frame.eyeGaze.leftY.littleEndianBytes)
    packet.append(frame.eyeGaze.rightX.littleEndianBytes)
    packet.append(frame.eyeGaze.rightY.littleEndianBytes)
    
    // Reserved (4 bytes of zeros)
    packet.append(contentsOf: [0x00, 0x00, 0x00, 0x00])
    
    // CRC32 checksum (optional, default zeros)
    let checksum = crc32(packet[0..<256])
    packet.append(checksum.littleEndianBytes)
    
    assert(packet.count == 260)
    return packet
}
```

### Pseudo-code: Packet Reading (C++ VCV)

```cpp
struct ARKitPacket {
    uint8_t magic[4];           // 0x57 0x52 0x44 0x03
    uint32_t sequenceNumber;
    float timestamp;
    float blendshapes[52];
    float headPose[6];          // pitch, yaw, roll, tx, ty, tz
    float eyeGaze[4];           // left_x, left_y, right_x, right_y
    uint32_t reserved;
    uint32_t checksum;
};

bool parsePacket(const uint8_t* data, ARKitPacket& out) {
    if (data[0] != 0x57 || data[1] != 0x52 || data[2] != 0x44) {
        return false;  // Invalid magic
    }
    
    if (data[3] != 0x03) {
        return false;  // Wrong version (expected v3)
    }
    
    memcpy(&out, data, sizeof(ARKitPacket));
    
    // Validate CRC32 if non-zero
    if (out.checksum != 0) {
        uint32_t computed = crc32(data, 256);
        if (computed != out.checksum) {
            return false;  // Checksum mismatch
        }
    }
    
    return true;
}
```

### Network Socket Setup (pseudo-code)

**iOS (Sender)**
```swift
let socket = try DatagramSocket(socketType: .udp)
try socket.setOption(.reuseAddress, value: true)
try socket.bind(address: "0.0.0.0", port: 0)  // Bind to any available port

// Send to unicast target
try socket.send(packet, to: targetIP, port: targetPort)

// OR broadcast mode
try socket.setOption(.broadcast, value: true)
try socket.send(packet, to: "255.255.255.255", port: 9000)
```

**VCV (Receiver)**
```cpp
int sock = socket(AF_INET, SOCK_DGRAM, 0);
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

sockaddr_in addr = {};
addr.sin_family = AF_INET;
addr.sin_port = htons(9000);
addr.sin_addr.s_addr = htonl(INADDR_ANY);

bind(sock, (sockaddr*)&addr, sizeof(addr));

uint8_t buffer[260];
sockaddr_in sender = {};
socklen_t senderLen = sizeof(sender);
recvfrom(sock, buffer, 260, 0, (sockaddr*)&sender, &senderLen);
```

---

## Error Handling

### Malformed Packets

**Detection**
- Magic header mismatch
- Version byte not 0x03 (fallback to v2 format)
- Packet size != 260 bytes
- CRC32 mismatch (if enabled)

**Action**
- Log warning with sender IP/port
- Increment error counter
- Discard packet, wait for next valid frame
- Display "Connection unstable" in UI if error rate > 5%

### Connection Loss

**Detection**
- No packet received for 5 seconds (timeout threshold)
- ICMP "unreachable" error

**Action**
- Set connection status to "Connecting"
- Attempt reconnect with exponential backoff (1s, 2s, 4s, max 30s)
- Show yellow/red status indicator in NERVE
- Continue buffering outgoing data (queue up to 300 packets)

### Network Jitter

**Handling**
- Accept packets out of order (sequence number is hint, not requirement)
- Ignore timestamp jumps > 1 second (system clock glitch)
- Apply low-pass smoothing (configurable 0–500ms) in app or module

---

## Future Extensions (v4.0+)

### Proposed v4.0 Features

1. **Hand Pose** (additional 30+ joints per hand)
2. **Full Body Tracking** (via companion body pose module)
3. **Multicast Support** (multiple receivers on single broadcast)
4. **Authenticated Sessions** (HMAC-SHA256 per packet)
5. **Compression** (optional zlib for bandwidth reduction)
6. **TCP Fallback** (for unreliable WiFi)

### Backward Compatibility

All v4.0+ receivers must:
- Check version byte (0x04, 0x05, etc.)
- Fall back to v3 interpretation if version unsupported
- Ignore unknown packet extensions (reserved bytes)

---

## Appendix: CRC32 Implementation

**Polynomial**: MPEG-2 (0x04C11DB7)

```cpp
uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i] << 24;
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ (crc & 0x80000000 ? 0x04C11DB7 : 0);
        }
    }
    
    return crc ^ 0xFFFFFFFF;
}
```

---

## Appendix: Example Packet Dump

Hexadecimal dump of a sample WeirdConnect v3 packet (first 32 bytes):

```
Offset  Hex                                      ASCII
00000   57 52 44 03 2A 00 00 00 78 56 34 12 AB  W R D . * . . . x V 4 . «
0000D   CD 3E 12 00 00 80 3F 00 00 80 3F 00 00  Í > . . . € ? . . € ? . .
0001A   40 3F 54 64 6A 3D 00 00 00 3D ...       @ ? T d j = . . . = ...
```

**Interpretation**
- `57 52 44 03`: Magic (WRD v3)
- `2A 00 00 00`: Sequence 42 (little-endian)
- `78 56 34 12`: Timestamp 0.375s (little-endian float32 ≈ 1.2365E-37)
- `AB CD 3E ...`: First blendshape (browDownLeft) ≈ 0.184
- ...continuing for 52 blendshapes + 6 head pose + 4 eye gaze

---

## Document Metadata

- **Version**: 3.0 (Current)
- **Status**: Production Ready
- **Last Updated**: 2026-02-23
- **Author**: WeirdSynths Core Team
- **Related**: SPEC.md, ARCHITECTURE.md (TBD)
- **License**: CC BY-SA 4.0 (open specification)
