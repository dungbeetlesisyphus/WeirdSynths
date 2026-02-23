# NERVE — Face Tracking to CV for VCV Rack

**Your face is a modular controller.**

NERVE receives real-time face landmark data over UDP and converts it into control voltage outputs in VCV Rack. Move your head, raise your eyebrows, open your mouth, blink — each gesture becomes a CV signal you can patch into anything.

Part of the [WeirdSynths](https://facefront.netlify.app) plugin collection.

## Outputs

NERVE provides 16 CV outputs from facial landmarks:

| Output | Signal | Range |
|--------|--------|-------|
| Head X | Yaw (left/right) | ±5V |
| Head Y | Pitch (up/down) | ±5V |
| Head Z | Roll (tilt) | ±5V |
| Distance | Face proximity | 0–10V |
| Left Eye | Left eye openness | 0–10V |
| Right Eye | Right eye openness | 0–10V |
| Gaze X | Horizontal gaze | ±5V |
| Gaze Y | Vertical gaze | ±5V |
| Mouth W | Mouth width | 0–10V |
| Mouth H | Mouth height | 0–10V |
| Jaw | Jaw openness | 0–10V |
| Lips | Lip distance | 0–10V |
| Brow L | Left eyebrow raise | 0–10V |
| Brow R | Right eyebrow raise | 0–10V |
| Blink | Blink trigger | 10V gate |
| Expression | Composite expression | 0–10V |

Additional derived outputs (Asymmetry, Intensity, Head Shake, Nod, Tension, Micro-expressions, Emotion) and gesture loop outputs are stubbed for future development.

## Controls

- **Smooth** — Slew limiter for CV outputs (0–500ms). Tames jitter from camera noise.
- **Scale** — Global output amplitude (0–100%).
- **Loop Len** — Gesture loop length (0.5–8s). *Future feature.*
- **Cam** — Enable/disable the UDP listener.
- **Rec** — Record gesture. *Future feature.*
- **Faces** — Single/multi-face mode toggle. *Future feature.*

## How It Works

NERVE listens for UDP packets on localhost port 9000 (configurable via right-click menu). A companion bridge application captures your webcam, runs face landmark detection, and sends the data to NERVE using a compact binary protocol.

### Architecture

```
Camera → Face Tracking → UDP (84 bytes) → NERVE → CV Outputs → Your Patch
```

### Bridge Options

**Python bridge (included):**

```bash
pip install mediapipe opencv-python
python bridge/nerve_bridge.py --show
```

This runs MediaPipe Face Mesh on your webcam and sends face data to NERVE at 30fps. Use `--show` to see a preview window. Use `--port` to change the UDP port.

**Raspberry Pi appliance (future):**

A dedicated Pi 5 + AI HAT+ running hardware-accelerated face tracking, sending data over your local network. Lower latency, no CPU impact on your music machine.

## Building from Source

Requires the [VCV Rack SDK](https://vcvrack.com/downloads).

```bash
git clone https://github.com/YOUR_USERNAME/WeirdSynths.git
cd WeirdSynths
make RACK_DIR=/path/to/Rack-SDK
```

### Install

Copy the built plugin to your VCV Rack plugins directory:

```bash
# macOS (Apple Silicon)
mkdir -p ~/Library/Application\ Support/Rack2/plugins-mac-arm64/WeirdSynths/res
cp plugin.dylib ~/Library/Application\ Support/Rack2/plugins-mac-arm64/WeirdSynths/
cp plugin.json ~/Library/Application\ Support/Rack2/plugins-mac-arm64/WeirdSynths/
cp res/Nerve.svg ~/Library/Application\ Support/Rack2/plugins-mac-arm64/WeirdSynths/res/

# macOS (Intel)
# Use plugins-mac-x64 instead of plugins-mac-arm64

# Linux
# Use ~/.local/share/Rack2/plugins-lin-x64/

# Windows
# Use %LOCALAPPDATA%/Rack2/plugins-win-x64/
```

Restart VCV Rack (Cmd+Q / Alt+F4, then relaunch).

## UDP Protocol

NERVE uses a compact 84-byte binary protocol:

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 4 | char[4] | Magic: `NERV` |
| 4 | 2 | uint16 LE | Protocol version: `1` |
| 6 | 2 | uint16 LE | Face count (1–4) |
| 8 | 68 | float32[17] LE | Face data (see output table) |
| 76 | 8 | uint64 LE | Timestamp (microseconds) |

All values are little-endian. Floats are normalized to -1..1 or 0..1 ranges. The module clamps all incoming values.

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

## Author

**Millicent** — [WeirdSynths](https://facefront.netlify.app)
