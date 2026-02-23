# WeirdOS v0.1.0

A custom Linux distribution that turns a Raspberry Pi 5 into a dedicated face-tracking modular synthesizer. Power on, point a camera at your face, and patch.

## What it does

WeirdOS boots directly into VCV Rack 2 with the WeirdSynths plugin pre-installed and the face tracking bridge running automatically. No desktop environment, no login screen, no distractions — just a modular synth that responds to your face.

**Boot sequence:** Power on → Camera detected → Bridge starts → JACK audio starts → VCV Rack launches → Patch.

## Hardware requirements

- **Raspberry Pi 5** (4GB or 8GB)
- **USB webcam** (any UVC-compatible camera)
- **USB audio interface** (or HDMI audio out)
- **microSD card** (16GB+, Class 10 / U3)
- **Display** (HDMI)
- **USB keyboard + mouse** (for patching)
- **Power supply** (USB-C, 5V 5A recommended)

## Quick start

### 1. Build the image

Requires a Linux x86_64 build host (Ubuntu 22.04+ recommended):

```bash
cd weirdos
make
```

First build takes 1-3 hours. Subsequent builds are incremental.

### 2. Flash to SD card

```bash
make flash DEVICE=/dev/sdX
```

Or manually:
```bash
./scripts/flash.sh build/output/images/weirdos-v0.1.0-rpi5.img /dev/sdX
```

### 3. Boot

Insert the SD card, connect camera + audio + display, power on.

**First boot** requires internet (Ethernet) to install MediaPipe. After that, it runs fully offline.

### 4. SSH access

```bash
ssh weird@weirdos.local
# password: weird
```

## CLI tool

WeirdOS includes a command-line tool for managing the system:

```
weirdos status    — check all services
weirdos restart   — restart bridge + VCV Rack
weirdos config    — edit system configuration
weirdos log       — view service logs
weirdos camera    — show camera info
weirdos audio     — show audio devices
weirdos version   — show version info
```

## Configuration

Edit `/etc/weirdos/weirdos.conf` (or `weirdos config` via SSH):

```bash
CAMERA_DEVICE="/dev/video0"    # webcam device
BRIDGE_PORT=9000               # UDP port for face data
AUDIO_DEVICE="hw:0"            # ALSA audio device
JACK_SAMPLERATE=48000          # sample rate
JACK_BUFSIZE=256               # buffer size (latency)
```

## Architecture

```
┌─────────────────────────────────────────────┐
│                WeirdOS v0.1.0               │
├─────────────────────────────────────────────┤
│  VCV Rack 2                                 │
│  ├── NERVE  (face → CV)                     │
│  ├── SKULL  (face → drums)                  │
│  ├── MIRROR (face display)                  │
│  └── VOICE  (vocal analysis)                │
├─────────────────────────────────────────────┤
│  Bridge (Python + MediaPipe + OpenCV)       │
│  └── UDP multicast → ports 9000-9002        │
├─────────────────────────────────────────────┤
│  JACK Audio ←→ ALSA                         │
│  V4L2 Camera ←→ USB                         │
│  Weston Compositor ←→ DRM/KMS               │
├─────────────────────────────────────────────┤
│  Linux 6.x (Buildroot, aarch64)             │
│  Raspberry Pi 5 (Cortex-A76, VideoCore VII) │
└─────────────────────────────────────────────┘
```

## Building from source

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt install build-essential wget bc python3 cpio unzip rsync \
    libncurses-dev file

# ~20GB free disk space needed
```

### Build options

```bash
make              # Build full image
make test         # Test boot in QEMU (no graphics)
make clean        # Remove build output
make distclean    # Full clean (re-download everything)
```

### Customizing the build

```bash
cd build/buildroot-*/
make O=../output BR2_EXTERNAL=../../buildroot/external menuconfig
```

## Project structure

```
weirdos/
├── Makefile                    # Top-level build
├── buildroot/
│   ├── configs/                # Buildroot defconfigs
│   │   └── weirdos_rpi5_defconfig
│   ├── external/               # BR2_EXTERNAL tree
│   │   ├── Config.in
│   │   ├── external.mk
│   │   ├── board/rpi5/         # Board-specific config
│   │   ├── package/            # Custom packages
│   │   │   ├── vcvrack/
│   │   │   ├── weirdsynths/
│   │   │   └── weirdsynths-bridge/
│   │   └── scripts/            # Build hooks
│   └── overlay/                # Root filesystem overlay
│       ├── etc/
│       │   ├── init.d/S99weirdos
│       │   └── weirdos/weirdos.conf
│       └── usr/bin/weirdos
├── scripts/
│   ├── build.sh
│   ├── flash.sh
│   └── test-qemu.sh
└── docs/
```

## License

GPL-3.0-or-later. Built on VCV Rack (GPL-3.0), MediaPipe (Apache 2.0), OpenCV (Apache 2.0), Buildroot (GPL-2.0).
