# VOICE — Module Specification

## 1. Module Overview

**Name:** VOICE
**Brand:** WeirdSynths
**Size:** 8 HP
**Role:** Hybrid — vocal analysis toolkit for pitch control + modulation extraction
**Character:** Smooth & musical — slightly smoothed outputs that feel melodic, not jittery

VOICE is a real-time vocal/audio analysis module that extracts pitch, amplitude, spectral brightness, onset transients, and harmonic content from any audio input. It completes the WeirdSynths body-as-instrument suite: NERVE (face→CV), SKULL (face→drums), MIRROR (face→display), VOICE (voice→pitch+modulation).

## 2. Design Goals

- **Accurate monophonic pitch tracking** with V/Oct output for driving oscillators
- **Harmonic series detection** output as polyphonic V/Oct channels
- **Musical smoothing** — no jitter, glides between pitches naturally
- **Low latency** — responsive enough for real-time performance
- **Visual feedback** — dot-matrix CRT-style pitch display (note name + octave)
- **Suite consistency** — matches WeirdSynths aesthetic (dark panel, green phosphor accents)
- **User-adjustable quality** — light/balanced/premium modes via context menu

## 3. Signal Flow

```
AUDIO IN ──┬──→ [Thru] ──→ AUDIO THRU (pass-through)
           │
           ├──→ [Gate Detector] ──→ GATE OUT (voiced/unvoiced)
           │
           ├──→ [YIN Pitch Detector] ──→ V/OCT OUT (monophonic fundamental)
           │         │
           │         └──→ [Harmonic Analyzer] ──→ HARMONICS OUT (polyphonic V/Oct)
           │
           ├──→ [Envelope Follower] ──→ ENV OUT (0-10V amplitude)
           │
           ├──→ [Onset Detector] ──→ ONSET OUT (trigger pulse)
           │
           └──→ [Spectral Brightness] ──→ BRIGHT OUT (0-10V spectral centroid)
```

## 4. I/O Table

### Inputs

| Port | Name | Signal Type | Description |
|------|------|-------------|-------------|
| IN | Audio In | Audio ±5V | Mono audio input — mic via VCV Audio, or any source |

### Outputs

| Port | Name | Signal Type | Description |
|------|------|-------------|-------------|
| THRU | Audio Thru | Audio ±5V | Pass-through of input signal, unchanged |
| V/OCT | Pitch | V/Oct | Detected fundamental pitch, 1V/octave standard |
| GATE | Gate | Gate 0-10V | High (10V) when voiced sound detected, 0V when silent/unvoiced |
| ENV | Envelope | CV 0-10V | Amplitude envelope follower output |
| ONSET | Onset | Trigger 10V | 1ms pulse on detected transients/onsets |
| BRIGHT | Brightness | CV 0-10V | Spectral centroid — dark vowels→low, bright consonants→high |
| HARM | Harmonics | Poly V/Oct | Polyphonic output: ch1=fundamental, ch2-N=detected harmonics |

## 5. Parameter Table

| Parameter | Control | Range | Default | Response | CV Input | Description |
|-----------|---------|-------|---------|----------|----------|-------------|
| SENS | Knob | 0–100% | 50% | Linear | No | Input sensitivity / voiced detection threshold |
| SMOOTH | Knob | 0–100% | 40% | Exponential | No | Pitch smoothing amount (0=raw, 100=heavy glide) |
| TONE | Knob | 0–100% | 50% | Linear | No | Brightness frequency threshold — sets crossover for BRIGHT output |

## 6. Visual Elements

### Dot-Matrix Pitch Display
- Same CRT aesthetic as MIRROR — green phosphor dots on dark background
- Shows detected **note name** (C, C#, D, ...) and **octave number** (0-8)
- Small **cents deviation** indicator (sharp/flat dots)
- "NO INPUT" flickering text when nothing connected or silent
- Phosphor persistence for smooth transitions between notes

### Activity LEDs
- Green LED next to GATE output — lit when voiced
- Small activity LEDs on ONSET output — flash on triggers

## 7. DSP Approach

### Pitch Detection: YIN Algorithm
- Autocorrelation-based with parabolic interpolation for sub-cent accuracy
- Buffer sizes by quality mode:
  - **Light:** 512 samples (~11ms @ 44.1kHz) — fast, good for speech
  - **Balanced:** 1024 samples (~23ms) — accurate, moderate latency
  - **Premium:** 2048 samples (~46ms) with multi-rate analysis — sub-cent, higher CPU
- Confidence threshold gates the output — no pitch jumps on uncertain detection
- V/Oct conversion: `voltage = log2(freq / 261.626)` (C4 = 0V)

### Harmonic Analysis
- After fundamental detected, analyze spectrum for harmonic peaks
- Output up to 8 polyphonic channels of V/Oct (fundamental + 7 harmonics)
- Set polyphony channel count to number of detected harmonics

### Envelope Follower
- RMS-based with configurable attack/release (auto-scaled from SMOOTH knob)
- Attack: 1ms (fast) to 50ms (smooth)
- Release: 10ms (fast) to 500ms (smooth)
- Output: 0-10V scaled to input level

### Onset Detection
- Spectral flux method — detects sudden increases in energy
- Threshold scaled by SENS knob
- Output: 10V trigger pulse, 1ms duration
- Minimum retrigger interval: 50ms (prevents double triggers)

### Spectral Brightness
- Spectral centroid calculation using FFT
- TONE knob shifts the reference frequency for normalization
- Output: 0-10V where 0V = very dark, 10V = very bright

### Voiced/Unvoiced Detection
- Combined analysis: pitch confidence + amplitude threshold
- SENS knob controls the threshold
- Hysteresis to prevent gate chatter at the threshold boundary
- Gate output: 10V when voiced, 0V when silent/unvoiced

## 8. Panel Layout (8 HP)

```
┌──────────────────┐
│   ╔════════════╗  │
│   ║            ║  │  ← CRT pitch display
│   ║   C#  4    ║  │    (note + octave + cents)
│   ║    +12¢    ║  │
│   ╚════════════╝  │
│                   │
│  [SENS]  [SMOOTH] │  ← Two knobs top row
│                   │
│      [TONE]       │  ← One knob center
│                   │
│  IN ○      ○ THRU │  ← Audio in/thru
│                   │
│  V/O ○     ○ GATE │  ← Pitch + gate
│                   │
│  ENV ○     ○ ONST │  ← Envelope + onset
│                   │
│  BRT ○     ○ HARM │  ← Brightness + harmonics
│                   │
│    W E I R D      │
│    S Y N T H S    │
└──────────────────┘
```

## 9. Bypass & Edge Cases

- **Bypass:** Passes audio from IN to THRU, all CV/gate outputs go to 0V
- **No input connected:** Display shows "NO INPUT" with flicker, all outputs 0V
- **Silence:** Gate goes low, pitch holds last detected value (doesn't jump)
- **Extreme pitch (very low):** Clamp V/Oct output below C0 (~16Hz)
- **Extreme pitch (very high):** Clamp V/Oct output above C8 (~4186Hz)
- **Noise input:** Gate stays low if confidence below threshold, no spurious triggers

## 10. Context Menu Options

- **Quality Mode:** Light / Balanced (default) / Premium
- **Pitch Range:** Bass (C1-C4) / Full (C1-C7, default) / Extended (C0-C8)
- **Harmonics Count:** 4 / 8 (default) polyphonic channels
- **Gate Hysteresis:** Low / Medium (default) / High
- **Connection status:** Shows "Active" or "No input"

## 11. Inspiration Notes

- Completes the WeirdSynths body-as-instrument paradigm
- Inspired by Intellijel Plonk's pitch tracking, Expert Sleepers Disting pitch detector
- The harmonic poly output is novel — nobody else outputs detected harmonics as playable V/Oct
- CRT pitch display ties into the MIRROR aesthetic language

## 12. Implementation Roadmap

1. **Panel SVG** — 8HP, dark, CRT display area, green accents
2. **DSP core** — YIN pitch detector with quality modes
3. **Envelope + onset + brightness** — parallel analysis chain
4. **Harmonic analyzer** — FFT peak detection
5. **Dot-matrix pitch display** — reuse CRT rendering from MIRROR
6. **Integration** — register in plugin files, build, test
7. **Tuning** — optimize smoothing curves, test with real vocals
