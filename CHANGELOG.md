# Changelog

All notable changes to WeirdSynths will be documented in this file.

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
versioning follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-02-23

### Added
- **NERVE** — Face landmark tracking to CV (20 HP). 468 MediaPipe landmarks converted to calibrated CV outputs: eyes, brows, jaw, mouth, head rotation, tongue, gaze. Polyphonic output bundles all channels.
- **SKULL** — Face-controlled drum machine (12 HP). Blink kicks, jaw snares, brow hi-hats with built-in analog-modeled percussion synthesis. Individual audio + trigger outputs plus mix bus.
- **MIRROR** — Dot matrix CRT face display (14 HP). Green phosphor wireframe rendering of face landmarks with adjustable brightness and persistence.
- **VOICE** — Vocal pitch tracker (8 HP). YIN pitch detection with V/Oct, gate, envelope, onset, brightness, and polyphonic harmonics outputs. Three quality modes (Light/Balanced/Premium). CRT pitch display.
- **Python bridge** (`bridge/nerve_bridge.py`) — MediaPipe face tracking with UDP broadcast to ports 9000 (NERVE), 9001 (SKULL), 9002 (MIRROR).
- Bridge protocol v2 — 21-float payload with tongue and brow data.

[2.0.0]: https://github.com/dungbeetlesisyphus/WeirdSynths/releases/tag/v2.0.0
