# WeirdPhone — Product Specification v0.1

## Overview

WeirdPhone is an iOS app that transforms any iPhone X or later into a biometric CV controller for VCV Rack, DAWs, and any OSC/MIDI software. Using ARKit's TrueDepth camera combined with Apple's Neural Engine, it captures 52 face blendshapes at 60fps with zero measurable CPU cost on the target audio computer. This document defines the complete product specification for version 0.1.

---

## Platform Requirements

- **iOS**: 17.0 or later
- **Hardware**: iPhone X, XS, XR, 11, 12, 13, 14, 15 (any model with TrueDepth front camera)
- **Network**: Target computer and iPhone must be on the same WiFi network
- **Audio Host**: VCV Rack v2.0+, DAWs with OSC/MIDI support, or custom OSC/MIDI applications
- **Latency Tolerance**: <8ms recommended for real-time control

---

## ARKit Face Data Specification

WeirdPhone captures 64 continuous control values at 60fps from ARKit's FaceTrackingProvider:

### Face Blendshape Coefficients (52 values)
All values are normalized to **[0.0, 1.0]** range, updated at 60fps via Apple's Neural Engine with hardware acceleration on A-series chips.

#### Complete ARKit Blendshape Mapping with Musical Use Cases

| Index | ARKit Name | Range | Musical Use Case | Free Tier | Pro Tier |
|-------|-----------|-------|------------------|-----------|----------|
| 0 | browDownLeft | 0–1 | Filter cutoff sweep, expression modulation | ❌ | ✓ |
| 1 | browDownRight | 0–1 | Filter cutoff sweep, expression modulation | ❌ | ✓ |
| 2 | browInnerUp | 0–1 | Surprise/impact modulation, AM depth | ❌ | ✓ |
| 3 | browOuterUpLeft | 0–1 | Pitch bend up, formant shift | ❌ | ✓ |
| 4 | browOuterUpRight | 0–1 | Pitch bend up, formant shift | ❌ | ✓ |
| 5 | cheekPuff | 0–1 | Reverb wet/dry, resonance modulation | ❌ | ✓ |
| 6 | cheekSquintLeft | 0–1 | Tremolo depth, pan modulation | ❌ | ✓ |
| 7 | cheekSquintRight | 0–1 | Tremolo depth, pan modulation | ❌ | ✓ |
| 8 | cheekRaiseLeft | 0–1 | Smile-driven filter sweep, warmth | ❌ | ✓ |
| 9 | cheekRaiseRight | 0–1 | Smile-driven filter sweep, warmth | ❌ | ✓ |
| 10 | chinRaise | 0–1 | Envelope attack/release, gate trigger | ❌ | ✓ |
| 11 | chinLowerDown | 0–1 | Pitch down, portamento | ❌ | ✓ |
| 12 | eyeBlinkLeft | 0–1 | **[Free Tier]** Gate on/off, sample hold | ✓ | ✓ |
| 13 | eyeBlinkRight | 0–1 | **[Free Tier]** Gate on/off, sample hold | ✓ | ✓ |
| 14 | eyeLookDownLeft | 0–1 | Y-axis (vertical) eye tracking, pitch mod | ❌ | ✓ |
| 15 | eyeLookDownRight | 0–1 | Y-axis (vertical) eye tracking, pitch mod | ❌ | ✓ |
| 16 | eyeLookInLeft | 0–1 | Convergence-driven effects, LFO rate | ❌ | ✓ |
| 17 | eyeLookInRight | 0–1 | Convergence-driven effects, LFO rate | ❌ | ✓ |
| 18 | eyeLookOutLeft | 0–1 | Divergence-driven effects, filter spread | ❌ | ✓ |
| 19 | eyeLookOutRight | 0–1 | Divergence-driven effects, filter spread | ❌ | ✓ |
| 20 | eyeLookUpLeft | 0–1 | Y-axis up eye tracking, pitch mod up | ❌ | ✓ |
| 21 | eyeLookUpRight | 0–1 | Y-axis up eye tracking, pitch mod up | ❌ | ✓ |
| 22 | eyeSquintLeft | 0–1 | Intensity/emphasis, distortion amount | ❌ | ✓ |
| 23 | eyeSquintRight | 0–1 | Intensity/emphasis, distortion amount | ❌ | ✓ |
| 24 | eyeWideLeft | 0–1 | Surprise, amplitude envelope open | ❌ | ✓ |
| 25 | eyeWideRight | 0–1 | Surprise, amplitude envelope open | ❌ | ✓ |
| 26 | jawForward | 0–1 | Filter sweep, consonant articulation | ❌ | ✓ |
| 27 | jawLeft | 0–1 | Pan left, L/R stereo width | ❌ | ✓ |
| 28 | jawRight | 0–1 | Pan right, L/R stereo width | ❌ | ✓ |
| 29 | jawOpen | 0–1 | **[Free Tier]** Amplitude envelope, gate trigger | ✓ | ✓ |
| 30 | mouthClose | 0–1 | Speech synthesis open/close, bright/dark | ❌ | ✓ |
| 31 | mouthFunnel | 0–1 | Formant shift down, vocal timbre | ❌ | ✓ |
| 32 | mouthPucker | 0–1 | Formant shift, vowel morphing | ❌ | ✓ |
| 33 | mouthLeft | 0–1 | Pan left, expression asymmetry | ❌ | ✓ |
| 34 | mouthRight | 0–1 | Pan right, expression asymmetry | ❌ | ✓ |
| 35 | mouthRollLower | 0–1 | Envelope decay, filter resonance | ❌ | ✓ |
| 36 | mouthRollUpper | 0–1 | Envelope attack, filter resonance | ❌ | ✓ |
| 37 | mouthShrugLower | 0–1 | Expression uncertainty, modulation depth | ❌ | ✓ |
| 38 | mouthShrugUpper | 0–1 | Expression uncertainty, modulation depth | ❌ | ✓ |
| 39 | mouthSmileLeft | 0–1 | **[Free Tier]** Smile sentiment, harmonic content | ✓ | ✓ |
| 40 | mouthSmileRight | 0–1 | **[Free Tier]** Smile sentiment, harmonic content | ✓ | ✓ |
| 41 | mouthFrownLeft | 0–1 | Sadness/minor mode, filter cutoff down | ❌ | ✓ |
| 42 | mouthFrownRight | 0–1 | Sadness/minor mode, filter cutoff down | ❌ | ✓ |
| 43 | mouthDimpleLeft | 0–1 | Subtle expression, micro-modulation | ❌ | ✓ |
| 44 | mouthDimpleRight | 0–1 | Subtle expression, micro-modulation | ❌ | ✓ |
| 45 | mouthUpperLipUp | 0–1 | Expression intensity, attack curve | ❌ | ✓ |
| 46 | mouthLowerLipDown | 0–1 | Expression intensity, release curve | ❌ | ✓ |
| 47 | mouthUpperLipSuckUpper | 0–1 | Tension/control, filter resonance peak | ❌ | ✓ |
| 48 | mouthLowerLipSuckLower | 0–1 | Tension/control, filter resonance peak | ❌ | ✓ |
| 49 | mouthUpperLipPress | 0–1 | Effort/dynamics, volume compression | ❌ | ✓ |
| 50 | mouthLowerLipPress | 0–1 | Effort/dynamics, volume compression | ❌ | ✓ |
| 51 | tongueOut | 0–1 | Playfulness, random modulation trigger | ❌ | ✓ |

### Head Pose (6 values)
Tracked at 60fps with 6 degrees of freedom:

| Index | Name | Range | Unit | Musical Use Case |
|-------|------|-------|------|------------------|
| 52 | headPitch | -π to +π | radians | Pitch up/down modulation, filter sweep |
| 53 | headYaw | -π to +π | radians | Pan modulation, stereo width control |
| 54 | headRoll | -π to +π | radians | Rotation effect, twisted distortion | 
| 55 | headTx | -1 to +1 | normalized | X position (left/right movement) |
| 56 | headTy | -1 to +1 | normalized | Y position (up/down movement) |
| 57 | headTz | -1 to +1 | normalized | Z position (depth, toward/away from camera) |

#### Head Pose Details
- **Pitch**: Head tilt forward/backward (nodding motion). Range: -π/2 to +π/2 radians.
- **Yaw**: Head turn left/right (shaking motion). Range: -π to +π radians.
- **Roll**: Head rotation around the forward axis. Range: -π/2 to +π/2 radians.
- **Tx, Ty, Tz**: Head center position normalized relative to camera frame. Useful for expressive gestural control.

### Eye Gaze (4 values)
Independent left and right eye gaze direction vectors:

| Index | Name | Range | Musical Use Case |
|-------|------|-------|------------------|
| 58 | eyeGazeLeftX | -1 to +1 | Horizontal gaze component (pan, filter) |
| 59 | eyeGazeLeftY | -1 to +1 | Vertical gaze component (pitch mod) |
| 60 | eyeGazeRightX | -1 to +1 | Horizontal gaze component (pan, filter) |
| 61 | eyeGazeRightY | -1 to +1 | Vertical gaze component (pitch mod) |

**Total: 64 values per frame at 60fps = 3,840 values/second of continuous control data.**

---

## Streaming Protocols

WeirdPhone supports three streaming protocols simultaneously over the same WiFi network. Users select the primary protocol; others mirror the data.

### Protocol 1: WeirdConnect UDP (Primary)

**Default Configuration**
- Port: `9000` (matches NERVE Pi bridge NERVE port for zero-config integration)
- Transport: UDP unicast or broadcast
- Frame Rate: 60 FPS (configurable in Settings)
- Latency: Optimized for <8ms end-to-end

**Packet Format**
- Binary little-endian format v3 (see PROTOCOL.md for full byte layout)
- Magic header: `0x57 0x52 0x44 0x03` ("WRD" + version 3)
- 260 bytes per packet
- Compatible with existing NERVE v2 modules (auto-detection via magic bytes)

**Advantages**
- Zero configuration with NERVE module
- Lowest latency (<8ms typical)
- Minimal bandwidth (260 bytes × 60 fps = 156 KB/s)
- Automatic fallback for NERVE v2 compatibility

### Protocol 2: OSC (Open Sound Control)

**Default Configuration**
- Port: `8000` TCP or UDP (configurable)
- Transport: OSC 1.0 over UDP (RFC 5379 compatible)
- Frame Rate: 60 FPS (configurable)

**Address Patterns**
- Blendshapes: `/weirdphone/blendshape/[name]` → Float 0.0–1.0
  - Example: `/weirdphone/blendshape/jawOpen`, `/weirdphone/blendshape/eyeBlinkLeft`
- Head Pose: `/weirdphone/head/[axis]` → Float (radians or normalized)
  - Example: `/weirdphone/head/pitch`, `/weirdphone/head/yaw`, `/weirdphone/head/tx`
- Eye Gaze: `/weirdphone/eye/[side]/[axis]` → Float -1.0–1.0
  - Example: `/weirdphone/eye/left/gazeX`, `/weirdphone/eye/right/gazeY`
- Meta: `/weirdphone/meta/[field]` → various types
  - `/weirdphone/meta/timestamp` (float, seconds)
  - `/weirdphone/meta/sequence` (int32)
  - `/weirdphone/meta/fps` (float)

**Advantages**
- Standard protocol; compatible with Max/MSP, PureData, Supercollider, TouchDesigner
- Human-readable addresses aid debugging
- Supports custom routing in DAWs (Reaper, Ableton via plugins)

### Protocol 3: Network MIDI

**Default Configuration**
- Transport: CoreMIDI network session (RTP-MIDI, RFC 6295)
- Channel: 1 (configurable)
- CC Range: 0–127

**CC Mapping**
- **CC 0–51**: Blendshapes (52 values)
  - CC 0 = browDownLeft, CC 1 = browDownRight, ..., CC 51 = tongueOut
  - Scaling: blendshape [0.0–1.0] → CC [0–127]
- **CC 52–57**: Head Pose (6 values)
  - CC 52 = headPitch (centered at 64, ±63 for ±π)
  - CC 53 = headYaw
  - CC 54 = headRoll
  - CC 55 = headTx
  - CC 56 = headTy
  - CC 57 = headTz
- **Note On/Off**: Blinking events
  - Note 60 (Middle C): Left eye blink
  - Note 61: Right eye blink
  - Velocity = open-close speed (0–127)

**Advantages**
- Native DAW integration (all major DAWs support CC/MIDI mapping)
- Familiar to musicians and producers
- Hardware synthesizer compatibility

---

## App Screens (5 Total)

### Screen 1: TRACK (Main View)

The primary face-tracking interface. User sees real-time face visualization with live meter feedback.

**Layout**
- **Top Section (50%)**: ARKit face visualization
  - Front-facing face model with blendshape deformation applied
  - Head pose axes (pitch/yaw/roll) overlaid as 3D rotation indicators
  - Eye gaze dots showing left/right pupil direction
  - Real-time FPS counter (top-right, dim text)
  - Connection status indicator (top-left): "Connected to [IP]:[port]" or "Offline"

- **Middle Section (40%)**: Live CV Meter Array
  - For Free Tier: 8 meters (jawOpen, headPitch, headYaw, headRoll, eyeBlinkL, eyeBlinkR, mouthSmileL, mouthSmileR)
  - For Pro: All 64 values in scrollable grid (8 columns × 8 rows)
  - Each meter shows: name (small text), value 0.0–1.0 (numeric), vertical bar (colored gradient)
  - Color coding: Green (inactive), Yellow (0.3–0.7), Red (active >0.7)

- **Bottom Section (10%)**: Control Bar
  - "TRACK" tab (active, highlighted)
  - "CONNECT", "MAP", "PRESETS", "SETTINGS" tabs
  - Record button (circle icon, toggles red when recording)
  - Gesture hint: "Drag to rotate face view"

**Interactions**
- Pinch to zoom face model
- Drag horizontal to rotate view
- Long-press to reset calibration
- Tap connection status to jump to CONNECT screen
- Auto-pause meters when app backgrounded

**Free Tier Restrictions**
- Only 8 meters visible
- Face model quality reduced
- "Upgrade to Pro" banner below TRACK tabs

---

### Screen 2: CONNECT (Network Configuration)

Configure streaming destination and protocol selection.

**Layout**
- **Header**: Large "CONNECT" title with status icon
  - Green dot = connected
  - Yellow dot = searching
  - Red dot = offline or error

- **Discovery Section** (scrollable list)
  - "Available NERVE Modules" list (auto-discovered via UDP broadcast on port 9001)
  - Each device: icon (computer or Pi), name, IP, last-seen timestamp
  - Tap to quick-connect (sets target IP/port, auto-selects WeirdConnect protocol)
  - Tap and hold to view details / manual override

- **Manual Configuration Section**
  - **Target IP**: Text field, keyboard input, recent list dropdown (last 5)
  - **Port**: Stepper (default 9000), numeric display
  - **Protocol**: Segmented control
    - "WeirdConnect" (default, orange)
    - "OSC" (blue)
    - "MIDI" (purple)
  - **Advanced** (disclosure triangle):
    - Broadcast mode (checkbox): Enable local broadcast (port 9000/9001)
    - Frame rate: Slider 10–60 FPS (snap to 30, 60)
    - Network timeout: Slider 5–30 seconds

- **Status Section**
  - Large status message: "Connected to 192.168.1.100:9000 (WeirdConnect, 60 FPS)" or "Waiting for target..."
  - Packet counter: "Sent: 14,872 packets, 0 lost"
  - Latency: "Last RTT: 3.2ms" (if supported by protocol)
  - "Test Connection" button (sends 10 test packets, shows result)

- **Bottom Navigation**
  - Back button (or swipe left)
  - Tabs: TRACK | CONNECT (active) | MAP | PRESETS | SETTINGS

**Interactions**
- Tap device in discovery list to auto-fill IP/port
- Manual IP entry with validation (checks IPv4 format)
- Toggle broadcast mode to enable/disable local discovery
- Connection persists across app restart
- Haptic feedback on connection success/failure

**Free Tier Restrictions**
- OSC and MIDI protocols show "Pro Feature" banner
- Can only use WeirdConnect protocol
- Discovery list limited to 5 devices

---

### Screen 3: MAP (Blendshape Mapping Editor)

Advanced mapping from 64 ARKit values to custom output ranges, curves, and MIDI CCs (Pro only).

**Layout**
- **Header**: "MAP" title + save/reset buttons

- **Navigation Tabs** (horizontal scroll)
  - Tabs for each category: "Face" (blendshapes), "Head Pose", "Eyes", "Presets"
  - Active tab highlighted in orange

- **Main Content Area** (for "Face" tab example)
  - **Vertical Scrollable List** of all 52 blendshapes
  - Each row shows:
    - Blendshape name (left, fixed width)
    - Current value (numeric, right-aligned)
    - Expand arrow (disclosure triangle)
  
- **Expanded Blendshape Row**
  - **Output Port**: Dropdown, options = "Off", "CV 1"–"CV 8" (or OSC address, or MIDI CC)
  - **Min/Max Range**: Two number fields, defaults 0.0–1.0
  - **Curve Type**: Segmented control
    - "Linear" (default)
    - "Exponential"
    - "Logarithmic"
    - "S-Curve" (smooth)
  - **Smoothing**: Slider 0–500ms (default 20ms)
  - **Invert**: Toggle checkbox
  - **Live Preview**: Waveform mini-graph showing output vs. input

- **Bottom Controls**
  - "Reset All to Default" button
  - "Save as Preset" button (saves current mapping to Presets screen)
  - Tabs: TRACK | CONNECT | MAP (active) | PRESETS | SETTINGS

**Interactions**
- Tap blendshape row to expand detailed controls
- Tap collapsed row to collapse
- Drag sliders to adjust min/max
- Number fields support long-press for fine tuning (±0.01 per tap)
- Live preview updates as user adjusts mappings
- Swipe right to reset individual blendshape to defaults

**Free Tier Restrictions**
- Map screen unavailable; shows "Pro Feature" modal
- Redirect to SETTINGS with upgrade prompt

---

### Screen 4: PRESETS (Saved Connection Configs)

Save and load complete connection + mapping configurations.

**Layout**
- **Header**: "PRESETS" title + "New Preset" button

- **Preset List** (scrollable)
  - Each preset card:
    - Name (editable, tap to rename)
    - Protocol icon + name (e.g., "WeirdConnect")
    - Target IP:port
    - Last modified date/time
    - Tap to load, swipe-left to delete with confirmation

- **Default Presets** (read-only, pin icon)
  - "NERVE Default" (WeirdConnect, localhost:9000)
  - "OSC Local" (OSC, localhost:8000)
  - "MIDI Network"

- **Save New Preset Dialog** (modal, appears after "New Preset" tap)
  - Text field: Preset name (required)
  - Checkbox: "Save current mapping?" (if Pro tier)
  - Two buttons: "Cancel", "Save"

- **Bottom Navigation**
  - Tabs: TRACK | CONNECT | MAP | PRESETS (active) | SETTINGS

**Interactions**
- Tap preset to load (updates CONNECT screen, shows brief toast "Loaded: [name]")
- Long-press to duplicate preset with name "[name] (copy)"
- Swipe-left to delete (undo available for 3 seconds)
- Rename by tapping the name field
- Share button (long-press): AirDrop / clipboard copy of preset JSON

**Free Tier Restrictions**
- Max 3 presets (shows "Upgrade to Pro for unlimited presets" banner)

---

### Screen 5: SETTINGS

App configuration, calibration, and account management.

**Layout**
- **Header**: "SETTINGS" title with close/back button

- **Sections** (scrollable)

1. **FaceTrack Settings**
   - Frame Rate: Slider 10–60 FPS (snap to 30, 60), numeric display
   - Smoothing: Slider 0–200ms (default 20ms)
   - Auto-Calibrate: Toggle (default on)
   - Calibrate Now: Button (captures current face as neutral, shows 3-second countdown)
   - Sensitivity: Slider 0.5–2.0x (how much blendshape changes needed to register)

2. **Network Settings**
   - Auto-Connect on Launch: Toggle
   - Last Connected: IP:port (tap to connect)
   - Network Timeout: Slider 5–30 seconds
   - WiFi Only: Toggle (prevents Bluetooth tethering)

3. **Output Configuration**
   - Protocol Display: Currently configured protocols with icons
   - Bandwidth Stats: "156 KB/s @ 60 FPS" (updates live)
   - Debug Mode: Toggle (enables packet logging, network graph)

4. **Account & Licensing**
   - Free Tier indicator (with icon)
   - "Upgrade to Pro" button → App Store
   - Upgrade price: "$9.99 (one-time)"
   - Restore Purchases: Button
   - App Version: "v0.1.0-beta"

5. **Misc**
   - Feedback: Button → email composer
   - Documentation: Button → Safari link
   - Privacy Policy: Link
   - Terms of Service: Link

- **Bottom Navigation**
  - Tabs: TRACK | CONNECT | MAP | PRESETS | SETTINGS (active)

**Interactions**
- All toggles use standard iOS switch components
- Sliders snap to nice intervals
- "Calibrate Now" initiates 3-second countdown with visual feedback
- "Upgrade to Pro" → App Store product sheet modal
- Debug Mode shows additional live stats overlay on TRACK screen

---

## Monetization Strategy

### Free Tier
- Limited to 8 core CVs:
  1. jawOpen
  2. headPitch
  3. headYaw
  4. headRoll
  5. eyeBlinkLeft
  6. eyeBlinkRight
  7. mouthSmileLeft
  8. mouthSmileRight
- All 3 protocols available (WeirdConnect, OSC, MIDI)
- TRACK, CONNECT, PRESETS, SETTINGS screens
- Max 3 saved presets
- Single protocol output (WeirdConnect default)
- Face visualization in TRACK (low quality)
- Continuous 30-day tracking of usage

### WeirdPhone Pro ($9.99 one-time IAP)
- **Unlock**: All 64 values (52 blendshapes + 6 head pose + 4 eye gaze)
- MAP screen (advanced blendshape mapping, curve editor, smoothing)
- Unlimited presets (save/load connection configs)
- MIDI CC full range mapping (0–127)
- High-quality face visualization
- Multi-protocol simultaneous output (e.g., WeirdConnect + OSC at once)
- Priority support / roadmap voting
- Offline mode (record session, export as CSV)
- No app limits or watermarks

### Upgrade Path
- Free → Pro via in-app purchase (In-App Purchases enabled, server-validated)
- Restore Purchases: User can restore on new device / reinstall
- Subscription: One-time payment model (no recurring charges)
- First-time Pro upgrade eligible for limited-time discount (e.g., $4.99 first 100 users)

---

## Technical Architecture

WeirdPhone uses a modern SwiftUI + ARKit stack with reactive Combine pipelines.

### High-Level Data Flow

```
[ARKit FaceTrackingProvider @ 60fps]
    ↓ (ARFaceGeometry + BlendShapes)
[ARKitManager] 
    ↓ (64 values per frame)
[StreamingManager] ← [MappingManager]
    ↓ (protocol dispatch)
[WeirdConnect UDP] [OSC UDP] [Network MIDI]
    ↓ (network stack)
[Target Computer] → [NERVE / DAW / Custom OSC]
```

### Three Core Manager Objects

#### 1. ARKitManager
**Responsibility**: Captures ARKit face data, applies smoothing, publishes updates.

- **Properties**:
  - `faceTrackingSession: ARKitFaceTrackingSession` (ARKit 6 API)
  - `smoothingFilter: SmoothingFilter` (configurable 0–500ms)
  - `currentFrameData: @Published FaceFrameData` (64 values)
  - `calibration: FaceCalibration` (neutral face baseline)

- **Methods**:
  - `startTracking() → Future<Void, Error>`
  - `stopTracking()`
  - `calibrateNow()` (captures current face as neutral)
  - `getBlendshape(_ name: String) → Float`
  - `getHeadPose() → HeadPose`
  - `getEyeGaze() → EyeGaze`

- **Combine Publishers**:
  - `$currentFrameData` → Updated at 60 FPS or configured rate
  - `frameRatePublisher` → Emits actual measured FPS every 1 second

#### 2. StreamingManager
**Responsibility**: Routes frame data to configured protocols, handles network I/O, manages connections.

- **Properties**:
  - `configuration: @Published StreamingConfig` (protocol, IP, port, etc.)
  - `connectionStatus: @Published ConnectionStatus` (connected / connecting / offline)
  - `isConnected: @Published Bool`
  - `outputChannels: [OutputChannel]` (WeirdConnect UDP, OSC, MIDI)
  - `packetStats: @Published PacketStats` (sent, lost, latency)

- **Methods**:
  - `connect(to config: StreamingConfig) → Future<Void, Error>`
  - `disconnect()`
  - `sendFrameData(_ data: FaceFrameData)`
  - `updateStreamingConfig(_ config: StreamingConfig)`
  - `startDiscovery()` (broadcasts on port 9001)
  - `discoverableDevices: @Published [DiscoveredDevice]`

- **Output Channels** (polymorphic):
  - `WeirdConnectUDP`: Sends 260-byte binary packets to [IP]:[port]
  - `OSCUDP`: Sends OSC messages to [IP]:[port]
  - `NetworkMIDI`: Sends MIDI CC/Note messages via CoreMIDI

#### 3. MappingManager
**Responsibility**: Applies user-defined mappings (curve, range, smoothing) to blendshapes before output (Pro tier).

- **Properties**:
  - `mappings: @Published [String: OutputMapping]` (blendshape name → mapping config)
  - `defaultMappings: [OutputMapping]` (identity mapping if user hasn't configured)

- **Data Structures**:
  ```swift
  struct OutputMapping {
    let blendshapeName: String
    let outputPort: OutputPort  // ".Off" / "CV1"–"CV8" / "OSC:address" / "MIDI:CC"
    let minValue: Float         // Output min (default 0.0)
    let maxValue: Float         // Output max (default 1.0)
    let curveType: CurveType    // .linear / .exponential / .logarithmic / .sCurve
    let smoothingMs: Int        // 0–500 ms
    let inverted: Bool          // Reverse output
  }
  
  enum CurveType {
    case linear
    case exponential(power: Float)
    case logarithmic
    case sCurve
  }
  ```

- **Methods**:
  - `applyMapping(_ blendshapeValue: Float, mapping: OutputMapping) → Float`
  - `loadMapping(from preset: PresetID)`
  - `saveMapping(as presetName: String)`
  - `resetToDefaults()`

### Data Types

```swift
struct FaceFrameData {
  let timestamp: TimeInterval         // Seconds since session start
  let sequenceNumber: UInt32          // Packet counter
  let blendshapes: [Float]            // 52 values, [0, 1]
  let headPose: HeadPose
  let eyeGaze: EyeGaze
}

struct HeadPose {
  let pitch: Float    // Radians, -π/2 to π/2
  let yaw: Float      // Radians, -π to π
  let roll: Float     // Radians, -π/2 to π/2
  let tx: Float       // Normalized, -1 to 1
  let ty: Float       // Normalized, -1 to 1
  let tz: Float       // Normalized, -1 to 1
}

struct EyeGaze {
  let leftX: Float    // -1 to 1
  let leftY: Float    // -1 to 1
  let rightX: Float   // -1 to 1
  let rightY: Float   // -1 to 1
}

struct StreamingConfig {
  let protocol: Protocol      // .weirdConnect / .osc / .midiNetwork
  let targetIP: String        // "192.168.1.100" or "localhost"
  let port: UInt16            // 9000, 8000, etc.
  let frameRateFps: Int       // 10–60, snapped to 30/60
  let broadcastMode: Bool     // Auto-discover enabled
}

enum Protocol {
  case weirdConnect           // UDP, 260-byte binary
  case osc                    // OSC 1.0 over UDP
  case midiNetwork            // CoreMIDI RTP-MIDI
}
```

### SwiftUI View Hierarchy

```
ContentView
├── ZStack {
│   ├── TabView {
│   │   ├── TrackTab (ARFaceView + MeterGrid)
│   │   ├── ConnectTab (DiscoveryList + ManualConfig)
│   │   ├── MapTab (BlendshapeEditor) — Pro only
│   │   ├── PresetsTab (PresetList)
│   │   └── SettingsTab (ScrollView + Forms)
│   │ }
│   └── ConnectionStatusBadge (overlay, top-left)
│ }
├── .environmentObject(ARKitManager)
├── .environmentObject(StreamingManager)
└── .environmentObject(MappingManager)
```

### Combine Signal Flow

```
ARKitManager.$currentFrameData
    .throttle(to: .seconds(1/60))  // 60 FPS
    .combineLatest(MappingManager.$mappings)
    .map { frame, mappings in
        applyMappings(frame, using: mappings)
    }
    .sink { mappedFrame in
        StreamingManager.sendFrameData(mappedFrame)
    }

StreamingManager.$connectionStatus
    .receive(on: DispatchQueue.main)
    .assign(to: &viewModel.$status)
```

### Error Handling

- **ARKit Errors**: Permission denied (camera), device unsupported (pre-X iPhone)
  - Show permission dialog, fallback UI
- **Network Errors**: Connection refused, timeout, malformed packets
  - Retry with exponential backoff (1s, 2s, 4s, max 30s)
  - Show yellow/red status indicator
- **MIDI Errors**: CoreMIDI connection lost
  - Attempt auto-reconnect every 5 seconds
  - Fallback to WeirdConnect or OSC

---

## Integration with NERVE Module (VCV Rack)

### NERVE Module Overview
NERVE is a VCV Rack module (part of the WeirdSynths plugin) that receives CV data from external sources via UDP. It currently supports WeirdConnect protocol v2 (legacy).

### Auto-Detection Mechanism
When NERVE v3 is loaded:
1. **Listens on port 9000** for UDP packets with magic bytes `0x57 0x52 0x44 0x??` ("WRD")
2. **Version check**: Reads 4th byte (version number)
   - `0x02` (v2): Legacy WeirdPhone or other WeirdConnect source
   - `0x03` (v3): WeirdPhone with ARKit face data
3. **Packet parsing**: Unpacks 260-byte payload (see PROTOCOL.md)
4. **Display**: Shows "WeirdConnect v3 — iPhone [name]" in module title

### NERVE v2 Backward Compatibility
- NERVE v2 module reads first 12 bytes (magic + sequence + timestamp)
- Falls back to reading bytes 12+ as raw CV data
- WeirdPhone v0.1 sends v3 packets; NERVE v2 ignores extra data, reads first 8 CVs

### NERVE Context Menu
- Right-click NERVE module
- Menu item: "Connect to WeirdPhone" → Opens browser dialog
  - Lists discovered WeirdPhone devices (auto-broadcast on 9001)
  - Click device → Auto-fill IP address in module
  - "Rescan" button to refresh list

### NERVE Output Jacks
**8 standard CV outputs** (±5V):
- **CV Out 0–7**: Hardwired to first 8 blendshapes (free tier)
  - 0: jawOpen
  - 1: headPitch (scaled ±π → ±5V)
  - 2: headYaw
  - 3: headRoll
  - 4: eyeBlinkLeft
  - 5: eyeBlinkRight
  - 6: mouthSmileLeft
  - 7: mouthSmileRight

**Expansion**: NERVE Pro (IAP) adds 8 more outputs:
- 8–15: mouthOpen, cheekPuff, eyeWideLeft, eyeWideRight, browDownLeft, browDownRight, browInnerUp, etc. (configurable)

### Upgrade Path
1. **User upgrades WeirdPhone to Pro** ($9.99 IAP)
2. **NERVE module detects v3 protocol** with full 64-value payload
3. **Optional**: User purchases NERVE Pro expansion (separate VCV module plugin)
4. **Result**: 16 CV outputs, MAP screen integration, full feature parity

---

## Future Roadmap (v0.2+)

- **Eye Tracking Precision**: Sub-degree gaze accuracy via per-user calibration
- **Hand Pose**: LearnARKit hand tracking extension (30+ finger joints)
- **Bluetooth Output**: Direct hardware synthesizer MIDI (without computer)
- **Recording/Playback**: Capture sessions as .weirdphone files, playback at variable speeds
- **Preset Cloud Sync**: iCloud-backed preset library
- **Telemetry**: Anonymous usage stats (opt-in) to guide feature development
- **WebRTC Streaming**: Stream ARKit data to web browser (Max/MSP via WebSocket)

---

## Appendix: Blendshape Recommendations by Use Case

### Ambient/Pad Synthesis
- `jawOpen` → Filter cutoff
- `cheekPuff` → Reverb wet/dry
- `headYaw` → Pan / stereo width
- `eyeLookUpLeft/Right` → Modulation depth

### Drum/Percussion Control
- `eyeBlinkLeft` → Kick trigger
- `eyeBlinkRight` → Snare trigger
- `mouthSmileLeft/Right` → Hi-hat open/close
- `jawOpen` → Drum resonance/pitch

### Vocal/Speech Synthesis
- `jawOpen` → Vowel height
- `mouthSmileLeft/Right` → Brightness (harmonic content)
- `mouthFunnel` → Formant frequency
- `mouthPucker` → Vowel shape

### Monophonic Bass
- `headPitch` → Pitch (±1 octave)
- `jawOpen` → Filter cutoff
- `mouthSmileLeft/Right` → Portamento/glide
- `eyeWideLeft/Right` → Amplitude envelope

### Granular/Sample Manipulation
- `browInnerUp` → Grain density
- `cheekSquintLeft/Right` → Grain pitch shift
- `tongueOut` → Random trigger
- `jawLeft/Right` → Spray / pan smear

---

## Document Metadata
- **Version**: 0.1 (Draft)
- **Status**: In Review
- **Last Updated**: 2026-02-23
- **Author**: WeirdSynths Core Team
- **Related**: PROTOCOL.md, ARCHITECTURE.md (TBD)
