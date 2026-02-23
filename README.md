# WeirdSynths — Your Face Is a Modular Controller

**Face tracking, vocal analysis, and body-as-instrument modules for VCV Rack 2.**

WeirdSynths turns your webcam and microphone into performance controllers. Raise an eyebrow to sweep a filter. Blink to trigger a kick drum. Sing to control an oscillator. See your face rendered in green phosphor.

## Modules

### NERVE — Face Tracking to CV (20 HP)

Receives 468 MediaPipe face landmarks via UDP and converts facial gestures into calibrated CV signals. Each facial region outputs independent voltages: eyes, brows, jaw, mouth, head rotation, tongue, and gaze.

**Outputs:** L Eye, R Eye, L Brow, R Brow, Jaw, Mouth W, Mouth H, Head X/Y/Z, Tongue, Gaze X/Y, Blink, Expression, Polyphonic bundle

**Controls:** Smooth (0–500ms), Scale (0–200%), Offset (±5V)

### SKULL — Face-Controlled Drums (12 HP)

Your face is a drum machine. Blinks trigger kicks, jaw movements fire snares, eyebrow raises hit hi-hats. Built-in analog-modeled percussion synthesis — no external sound sources needed.

**Outputs:** Kick (audio + trigger), Snare (audio + trigger), Hat (audio + trigger), Mix

**Controls:** Kick Tune/Decay, Snare Tone/Decay, Hat Tone/Decay, Sensitivity

### MIRROR — CRT Face Display (14 HP)

Dot-matrix display renders your face as a green phosphor wireframe in real-time. Visual feedback for face tracking performance, and an aesthetic statement in your rack.

**Controls:** Brightness, Persistence

### VOICE — Vocal Pitch Tracker (8 HP)

Real-time audio analysis using the YIN pitch detection algorithm. Extracts pitch (V/Oct), gate, amplitude envelope, onset transients, spectral brightness, and harmonic series from any monophonic audio input. No bridge required — works with any VCV audio source.

**Inputs:** Audio In

**Outputs:** Audio Thru, V/Oct, Gate, Envelope (0–10V), Onset (trigger), Brightness (0–10V), Harmonics (polyphonic V/Oct)

**Controls:** Sensitivity, Smoothing, Tone

**Quality modes** (right-click): Light (512 samples, ~11ms), Balanced (1024, ~23ms), Premium (2048, ~46ms)

## Quick Start

### Prerequisites

- [VCV Rack 2](https://vcvrack.com) (Free or Pro)
- Python 3.9+ (for face tracking bridge)
- Webcam (any USB or built-in)

### Install the Plugin

Download the latest release from [GitHub Releases](https://github.com/dungbeetlesisyphus/WeirdSynths/releases) and copy it to your VCV Rack plugins folder:

```bash
# macOS (Apple Silicon)
cp -r WeirdSynths ~/Library/Application\ Support/Rack2/plugins-mac-arm64/

# macOS (Intel)
cp -r WeirdSynths ~/Library/Application\ Support/Rack2/plugins-mac-x64/

# Linux
cp -r WeirdSynths ~/.Rack2/plugins/

# Windows
copy WeirdSynths %LOCALAPPDATA%\Rack2\plugins\
```

Restart VCV Rack completely (Cmd+Q / Alt+F4, then relaunch).

### Start the Face Tracking Bridge

```bash
pip3 install mediapipe opencv-python numpy
python3 bridge/nerve_bridge.py
```

The bridge sends face data to NERVE (port 9000), SKULL (port 9001), and MIRROR (port 9002) simultaneously.

### VOICE — No Bridge Needed

VOICE analyzes audio directly through VCV's audio input. Patch any audio source into VOICE's IN jack and connect V/OCT to an oscillator to start singing notes into your synth.

## Building from Source

Requires the [VCV Rack SDK](https://vcvrack.com/downloads).

```bash
git clone https://github.com/dungbeetlesisyphus/WeirdSynths.git
cd WeirdSynths
make RACK_DIR=/path/to/Rack-SDK
make install
```

## Bridge Protocol

The Python bridge uses UDP with a compact binary protocol:

| Offset | Size | Type | Field |
|--------|------|------|-------|
| 0 | 4 | char[4] | Magic: `NERV` |
| 4 | 2 | uint16 LE | Protocol version: `2` |
| 6 | 2 | uint16 LE | Face count (1–4) |
| 8–88 | 84 | float32[21] LE | 21 face parameters |
| 88 | 8 | uint64 LE | Timestamp (microseconds) |

Bridge broadcasts to ports 9000, 9001, 9002 by default. Configurable with `--port`.

## Documentation

- [Module Reference](https://dungbeetlesisyphus.github.io/WeirdSynths/modules.html) — Full I/O tables, parameters, and patch tips
- [Getting Started Guide](https://dungbeetlesisyphus.github.io/WeirdSynths/guide.html) — Step-by-step setup and first patches
- [Dev Board](https://dungbeetlesisyphus.github.io/WeirdSynths/board.html) — Public roadmap and task tracker

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).

The Python bridge (`bridge/nerve_bridge.py`) uses MediaPipe (Apache 2.0) and OpenCV (Apache 2.0).

## Author

**Millicent** — [WeirdSynths](https://dungbeetlesisyphus.github.io/WeirdSynths/)
