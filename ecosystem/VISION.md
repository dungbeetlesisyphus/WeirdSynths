# WeirdSynths Ecosystem Vision
### Platform Strategy · Hardware · AI · Software · Revenue

---

## 1. The Big Picture

WeirdSynths began as a VCV Rack plugin. It becomes a **platform** — the connective tissue between your body, an intelligent machine, and your modular synthesizer.

The Raspberry Pi 5 + AI HAT+ 2 (Hailo-10H, 40 TOPS, 8GB on-board RAM) unlocks something genuinely new: a **sub-$200 device** that runs real-time biometric tracking, local LLMs for code generation, offline synthesis AI, AND ships fully preloaded with the tools to build your own modules. No cloud subscription required. No privacy concerns. Plug in, power up, make music.

The platform has three layers:

```
┌─────────────────────────────────────────────────────────┐
│  SOFTWARE LAYER                                         │
│  VCV Rack Plugins · WeirdPhone · WeirdStudio (Web UI)   │
├─────────────────────────────────────────────────────────┤
│  INTELLIGENCE LAYER                                     │
│  Local LLM (Qwen2.5-Coder) · Cloud AI (Claude API)     │
│  Biometric Models · Neural Synthesis Models             │
├─────────────────────────────────────────────────────────┤
│  HARDWARE LAYER                                         │
│  WeirdBox (Pi 5 + AI HAT+ 2) · WeirdPhone · WeirdFace  │
└─────────────────────────────────────────────────────────┘
```

Everything connects over **WeirdConnect** — a single open protocol.

---

## 2. Hardware Product Line

### 2.1 WeirdBox Standard — $249

The entry point. Everything you need to run the full biometric tracking suite and start making music with your body.

| Component | Spec |
|---|---|
| Compute | Raspberry Pi 5 (8GB RAM) |
| AI Accelerator | Raspberry Pi AI HAT+ 2 |
| NPU | Hailo-10H (40 TOPS INT4) |
| On-board RAM | 8GB LPDDR4X (HAT-local, zero impact to Pi RAM) |
| Storage | 64GB industrial microSD |
| Camera | WeirdCam (Pi Camera Module 3, 12MP, 120fps crop) |
| Connectivity | WiFi 5, Gigabit Ethernet, 2× USB 3.0 |
| Power | USB-C PD, 5V/5A |
| Enclosure | Anodized aluminum, 90×60×30mm |
| OS | WeirdOS (Buildroot Linux, custom) |

**What it ships with:**
- Full biometric tracking suite (face 478-point, body, hands, gaze, emotion, phoneme)
- NERVE, SKULL, VOICE, MIRROR, BODY, HAND, GAZE, EMOTION VCV Rack modules
- WeirdStudio web UI (module builder, idea feed, patch library)
- **WeirdMaker** — local VCV Rack module factory powered by Qwen2.5-Coder 1.5B
- 1-year WeirdConnect cloud sync (optional, opt-in)

---

### 2.2 WeirdBox Pro — $349

Same hardware, expanded storage and pre-configured for the full model suite.

| Delta from Standard | Pro Spec |
|---|---|
| Storage | 256GB NVMe (via Pi 5 PCIe) |
| Models | Full model zoo pre-installed (tracking + synthesis AI) |
| WeirdMaker | Enhanced with Qwen2.5-Coder 1.5B + Qwen2 VL (vision-language) |
| Extras | Aluminum rack ears, DIN rail mount option |

**Adds:**
- TIMBRE (DDSP voice-to-instrument)
- SPLIT (Demucs source separation)
- FORGE (HiFiGAN neural vocoder)
- IRON (neural amp modeling)
- ROOM (visual acoustics)
- PRISM (spectral descriptors)
- Offline VCV Rack patch rendering (headless)

---

### 2.3 WeirdBox DIY — $129

Just the image + module binaries. Bring your own Pi 5 and AI HAT+ 2.

For tinkerers who already have the hardware. Full WeirdOS image, all modules, everything identical to Standard. No enclosure, no camera.

---

### 2.4 WeirdPhone — Free + Pro ($9.99)

The iPhone companion. ARKit TrueDepth, 52 blendshapes at 60fps. Zero CPU cost — runs entirely on the Neural Engine.

Full spec: `weirdphone/docs/SPEC.md`

---

### 2.5 WeirdFace (Future, ~2026 Q3) — $449

Dedicated standalone Eurorack face tracker in 4HP. Hailo-8L on-board, camera built in. No phone, no Pi, no WiFi — pure CV output from 8 configurable blendshapes. Eurorack-native power (±12V), ribbon cable expander, front panel jack array.

---

## 3. Software Products

### 3.1 WeirdSynths Plugin Bundle

| Tier | Price | Included |
|---|---|---|
| Free | Free | NERVE (auto-discover, 8 CVs) |
| Core | $19 | NERVE + SKULL + VOICE |
| Full | $39 | All 8+ modules |
| WeirdBox | Bundled | All current + future modules, forever |

Available on VCV Library. WeirdBox owners get a serial key at purchase that unlocks Full tier.

---

### 3.2 WeirdStudio (Browser UI, runs on WeirdBox)

Accessed at `http://weirdbox.local` from any device on the same network.

Five sections:

1. **Dashboard** — live biometric feeds, connection status, latency monitor
2. **Module Maker** (WeirdMaker) — AI-assisted VCV Rack module builder
3. **Ideas Feed** — AI-generated module concept cards, rate/save/build
4. **Patch Library** — save/share/recall VCV Rack patches with preset snapshots
5. **Settings** — AI backend, network, model management, storage

---

### 3.3 WeirdMaker — The On-Device Module Factory

This is the centerpiece feature of WeirdBox. It ships the full **VCV Module Factory** workflow — 150+ skills covering DSP, panel design, optimization, submission — running on a local LLM.

**What it can do:**
- Generate complete VCV Rack C++ module code from a description
- Design panel SVGs with correct dimensions and color systems
- Suggest DSP algorithms, explain trade-offs, write test harnesses
- Estimate CPU budget and warn about performance risks
- Generate user manuals and changelog entries
- Walk through the full 12-step module-to-library pipeline

**How it works:**
- Qwen2.5-Coder 1.5B runs on the Hailo-10H's 8GB RAM (entirely off-Pi)
- Pi 5 serves WeirdStudio UI, handles file I/O, runs the build toolchain
- ARM64 GCC cross-compiler pre-installed for Rack module compilation
- Results appear in the browser UI, files saved to connected USB or network share

**Tech stack:**
```
Browser (WeirdStudio)
  → FastAPI (Pi 5, Python)
  → hailort Python bindings (Hailo-10H, 8GB on-board)
  → Qwen2.5-Coder 1.5B (.hef compiled for Hailo-10H)
  → Context: VCV Rack API headers + vcv-module-factory skill system
```

**The skills system:**
The vcv-module-factory skill library (currently used in this development session) becomes the **context injection layer** for WeirdMaker. Each skill file is embedded as retrieval context, loaded on-demand based on the current task. The local LLM never needs to know about all 150 skills at once — only the relevant ones for the current question are injected, keeping context tight and generation fast.

---

## 4. The AI Backend: Local vs Cloud vs Hybrid

This is a first-boot setup choice, revisitable anytime in Settings.

### 4.1 Setup Wizard — First Boot

After WeirdBox powers on for the first time, WeirdStudio shows a 3-step wizard:

```
Step 1: Welcome to WeirdBox
Step 2: Choose your AI brain  ← THE KEY CHOICE
Step 3: Connect to VCV Rack
```

**Step 2 screen:**

```
┌─────────────────────────────────────────────────────────────┐
│  HOW SHOULD WEIRDMAKER THINK?                               │
│                                                             │
│  ◉ LOCAL (Recommended for most users)                       │
│    Qwen2.5-Coder runs directly on your WeirdBox             │
│    ✓ Fully offline — works anywhere, no internet needed     │
│    ✓ Private — your code never leaves your device           │
│    ✓ No API costs, no subscription                          │
│    ↻ Slightly slower responses (~3-8s per generation)       │
│    ~ Good for: module scaffolding, DSP code, quick ideas    │
│                                                             │
│  ○ CLOUD (Best quality)                                     │
│    Routes through Claude API (Anthropic)                    │
│    ✓ Highest quality responses                              │
│    ✓ Latest knowledge, broader context                      │
│    ✗ Requires internet connection                           │
│    ✗ API key required (pay-as-you-go, ~$0.01-0.10/query)   │
│    ~ Good for: complex DSP, debugging, documentation        │
│                                                             │
│  ○ HYBRID (Power users)                                     │
│    Local first, escalate to cloud when needed               │
│    ✓ Fast for simple tasks, powerful for complex ones       │
│    ✓ Works offline, enhanced when connected                 │
│    ~ WeirdMaker decides which to use based on task          │
│    ~ Requires API key for cloud escalation                  │
│                                                             │
│                              [ Set Up Local →  ]           │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 Local Mode — Qwen2.5-Coder 1.5B on Hailo-10H

The model runs as a persistent background service on WeirdOS, loaded into the HAT's 8GB RAM at boot.

- **Model**: Qwen2.5-Coder 1.5B quantized to INT4, compiled to `.hef` for Hailo-10H
- **Context window**: ~32k tokens (enough for a full module + all relevant skill context)
- **Throughput**: ~20-40 tokens/second on Hailo-10H
- **Expected response time**: 3-8 seconds for short generations, 15-30s for full modules
- **Privacy**: Zero data leaves device. No phone home, no telemetry (opt-in only)
- **Offline**: Works without any network connection

The skill injection system compensates for the smaller model size. Rather than relying on the model to "remember" VCV Rack API internals, the relevant skills are injected directly into the prompt context for each query. This means the 1.5B model performs well above its parameter count for WeirdMaker tasks.

### 4.3 Cloud Mode — Claude API

- **Model**: Claude (latest available via Anthropic API)
- **API key**: User provides their own Anthropic API key, stored encrypted on device
- **Estimated cost**: $0.01–0.05 for a simple DSP function, $0.05–0.15 for a full module
- **Speed**: 2-5 seconds (API latency)
- **Quality**: Noticeably higher — better at complex DSP math, multi-file refactoring, debugging
- **Requirements**: Active internet, valid API key, sufficient balance

### 4.4 Hybrid Mode — Smart Routing

WeirdMaker classifies each task before sending it:

| Task Type | Routing | Rationale |
|---|---|---|
| "Scaffold a basic VCA module" | Local | Template generation, skill context is enough |
| "What's the best anti-aliasing for this oscillator?" | Local | Skill injection handles it |
| "Debug this segfault in my DSP" | Cloud | Requires multi-step reasoning |
| "Explain ladder filter math" | Local | Well-covered in skills |
| "Port this Max/MSP patch to C++" | Cloud | Broad knowledge needed |
| "Generate panel SVG" | Local | Template + skill context |
| "Optimize for ARM NEON SIMD" | Cloud | Architecture-specific expertise |

The routing logic is configurable in Settings — you can force-local, force-cloud, or let WeirdMaker decide. A small indicator in the UI always shows which backend handled the last query.

---

## 5. Offline vs Online Features

### 5.1 Always Works Offline (No Internet Required)

These are the **core value proposition**. A WeirdBox without internet is still a complete, useful product.

- Full biometric tracking suite (face, body, hands, gaze, emotion, phoneme, depth)
- All VCV Rack module CV output via WeirdConnect
- WeirdPhone companion (streams over local WiFi)
- WeirdMaker (Local mode) — complete module factory
- Ideas Feed (reads from local `ideas.json`, offline rating)
- WeirdStudio UI (served from Pi itself)
- Patch Library (local storage)
- Preset save/recall
- All synthesis AI modules (TIMBRE, SPLIT, FORGE, IRON on Pro) — models run on HAT
- Offline VCV Rack headless rendering (Pro)
- Firmware stored on-device, no cloud activation required

**Design principle:** WeirdBox must work as well on a tour bus in the desert as it does in a studio with fiber internet. Every core feature is offline-first.

### 5.2 Enhanced with Internet (Optional, Opt-In)

These features activate when connected but are never required:

- **WeirdMaker Cloud** — route queries to Claude API (better quality)
- **Ideas Feed AI** — fetch fresh AI-generated module ideas from WeirdSynths server (cron: daily)
- **Patch Library Sync** — back up patches to cloud, share between devices
- **Firmware OTA** — one-click WeirdOS updates
- **Model Zoo Updates** — newer/better tracking models pushed to device
- **WeirdConnect Relay** — route WeirdPhone/WeirdBox CV over internet to a remote DAW
- **Analytics** (opt-in) — usage patterns help improve future ideas and defaults
- **Community Patches** — browse and download patches from other WeirdBox users

### 5.3 Offline-First Architecture

WeirdOS uses a **local-primary, sync-on-connect** pattern:

```
All data writes → local SQLite/JSON first
                → queue for sync
                → flush when internet available
```

No feature ever blocks on network. Network errors are silent — everything falls back to local state.

---

## 6. TOPS Budget — AI HAT+ 2 (40 TOPS)

The Hailo-10H runs two classes of workloads:

| Workload Type | Budget | Allocation |
|---|---|---|
| Real-time inference (tracking) | ~14 TOPS | Always-on biometric models |
| Batch inference (LLM / synthesis AI) | ~24 TOPS | On-demand, queued |

### 6.1 Real-Time Tracking Suite (~14 TOPS)

| Model | TOPS Est. | Output |
|---|---|---|
| Face 478-point (MediaPipe GHUM) | 3.5 | Blendshapes + head 6DOF |
| Phoneme detector (wav2vec lite) | 2.0 | 44 phoneme probabilities |
| Emotion classifier (AffectNet) | 1.5 | 7 affect scores |
| Body pose 33-point (BlazePose Heavy) | 3.0 | Skeleton + velocity |
| Hand landmarks 42-point (2 hands) | 2.0 | Per-finger positions + curl |
| Gaze vector (L2CS-Net) | 1.0 | Yaw/pitch + saccade |
| Depth estimation (MiDaS lite) | 1.0 | Foreground distance |
| **Total** | **14.0** | |

### 6.2 LLM + Synthesis AI (~24 TOPS available for batch)

| Model | TOPS Est. | Mode |
|---|---|---|
| Qwen2.5-Coder 1.5B INT4 | 8-12 (burst) | WeirdMaker queries |
| DDSP instrument encoder (TIMBRE) | 4.0 | On-trigger, per note |
| Demucs source separator (SPLIT) | 6.0 | Per audio segment |
| HiFiGAN lite vocoder (FORGE) | 3.0 | On-trigger |
| Neural amp model (IRON) | 1.5 | Always-on (stream mode) |
| Room acoustics estimator (ROOM) | 1.5 | Per frame (low rate) |

The LLM and synthesis models are never simultaneously pegged — they're task-queued. WeirdOS uses a HAL (Hardware Abstraction Layer) scheduler that manages priority:

```
Priority 1: Real-time tracking suite (always pre-empts)
Priority 2: Active synthesis AI (IRON stream mode if active)
Priority 3: LLM inference (background, can be interrupted)
Priority 4: Synthesis AI batch jobs (TIMBRE, SPLIT, FORGE)
```

---

## 7. WeirdConnect Platform

**WeirdConnect** is the open protocol that binds everything together. Version 3 is documented in `weirdphone/docs/PROTOCOL.md`.

The platform vision extends this to:

### 7.1 WeirdConnect Hub

A discovery and routing layer running on WeirdBox:

- Auto-discovers all WeirdConnect devices on the LAN (WeirdPhone, WeirdFace, other WeirdBoxes)
- Routes CV streams to correct VCV Rack modules
- Merges streams from multiple sources (e.g. face + hands from two different iPhones)
- Provides OSC bridge for DAWs (Ableton, Logic, Bitwig, Reaper)
- MIDI bridge for hardware gear

### 7.2 WeirdConnect Relay (Online)

Routes CV data over the internet for remote collaboration. Two WeirdBox users in different cities can share biometric control of the same patch. Latency-managed (50-100ms target, with jitter buffer).

### 7.3 WeirdConnect SDK

Open SDK for third-party developers to:
- Build apps that speak WeirdConnect protocol
- Add NERVE-compatible modules in their own VCV plugins
- Integrate WeirdBox biometrics into their DAW plugins or scripts

SDK published under MIT. Protocol documentation is already public (`PROTOCOL.md`).

---

## 8. Ideas Feed — AI-Powered Module Concepts

The Ideas Feed (`docs/ideas.html`) is a CRT-aesthetic rating interface for AI-generated module ideas. It closes the loop between the device and the module factory:

```
WeirdBox generates ideas (local LLM, daily)
  → User rates on ideas.html (Skip / Interesting / Build This)
  → "Build This" triggers WeirdMaker
  → WeirdMaker generates full module scaffold + panel
  → One-click compile on WeirdBox
  → Install directly to connected VCV Rack
```

**Idea generation pipeline:**
1. Qwen2.5-Coder receives a prompt seeded with: current module catalog, user's build history, trending tags from community patches
2. Generates 5 new ideas per day (local, offline)
3. If cloud enabled: also fetches 5 ideas from WeirdSynths server (human-curated + AI-generated)
4. Results merged, deduped, appended to `ideas.json`

**The feedback loop:** rated ideas improve future generation. "Build This" items are weighted higher in prompt context. "Skip" items seed negative examples.

---

## 9. Full Product & Revenue Model

### 9.1 Product Tree

```
WeirdSynths
├── Hardware
│   ├── WeirdBox Standard ($249)
│   ├── WeirdBox Pro ($349)
│   ├── WeirdBox DIY ($129)
│   └── WeirdFace Eurorack (future, $449)
│
├── iOS
│   └── WeirdPhone (Free + Pro $9.99)
│
├── VCV Rack
│   ├── Free tier (NERVE)
│   ├── Core bundle ($19)
│   └── Full bundle ($39)
│
├── Software Services
│   ├── WeirdConnect Relay ($4.99/mo or $39/year)
│   ├── Patch Library Cloud Sync ($2.99/mo or $24/year)
│   └── Ideas Feed Pro (5 community ideas/day, $1.99/mo)
│
└── Content
    ├── WeirdSynths Academy (courses, future)
    └── Community Patch Store (rev share, future)
```

### 9.2 Revenue Projections (Month 12)

| Stream | Volume | Price | Monthly Rev |
|---|---|---|---|
| WeirdBox Standard | 80 units | $249 | $19,920 |
| WeirdBox Pro | 30 units | $349 | $10,470 |
| WeirdBox DIY | 40 units | $129 | $5,160 |
| WeirdPhone Pro | 400 subs | $9.99 | $3,996 |
| VCV Full Bundle | 60/mo | $39 | $2,340 |
| VCV Core Bundle | 120/mo | $19 | $2,280 |
| WeirdConnect Relay | 80 subs | $4.99/mo | $399 |
| Cloud Sync | 120 subs | $2.99/mo | $359 |
| **Total** | | | **~$44,924/mo** |

Year 1 total (conservative ramp): **~$280K**
Year 2 (with WeirdFace, Academy, community): **~$600K+**

### 9.3 Unit Economics — WeirdBox Standard

| Item | Cost |
|---|---|
| Pi 5 (8GB) | $80 |
| AI HAT+ 2 | $130 |
| Camera Module 3 | $25 |
| 64GB industrial SD | $12 |
| Enclosure + PCB | $18 |
| Assembly + QC | $10 |
| Shipping + packaging | $8 |
| **COGS** | **$283** |

At $249 retail — this is a loss. The box is **subsidized** by software revenue (phone app, relay, sync, VCV bundles). This is the classic hardware-as-distribution play.

**Path to margin:** negotiate volume pricing on Pi 5 + HAT (direct from Raspberry Pi Trading at 100+ units), shift to custom PCB carrier board at scale (~$40 total PCIe + USB hub + power management vs current BOM).

Target COGS at 500 units/year: **$165**, margin at $249 = **34%**.

---

## 10. Build Sequence / Roadmap

### Phase 1 — Foundation (Now → Month 3)

Everything offline, core experience working end-to-end.

- [ ] WeirdOS v1.0 image — Pi 5 + AI HAT+ 2, Hailo-10H drivers, GStreamer pipeline
- [ ] Tracking bridge — all 7 models running, multi-port UDP to VCV
- [ ] NERVE, SKULL, VOICE, MIRROR — build verified, VCV Library submitted
- [ ] WeirdPhone v1.0 — App Store submission (ARKit, Free + Pro tiers)
- [ ] WeirdMaker v0.1 — Qwen2.5-Coder local, basic code generation, WeirdStudio UI
- [ ] Ideas Feed — online, ideas.json + ideas.html deployed

### Phase 2 — Intelligence (Month 3 → Month 6)

Local LLM mature, synthesis AI modules, first hardware unit.

- [ ] WeirdMaker v1.0 — full 12-step module factory pipeline, cloud/local/hybrid modes
- [ ] Synthesis AI modules — TIMBRE, SPLIT, FORGE, IRON (Pro tier)
- [ ] BODY, HAND, GAZE, EMOTION VCV modules — full implementations
- [ ] WeirdBox first production run (50 units, Standard + Pro)
- [ ] WeirdConnect Hub — multi-device discovery and routing
- [ ] First-boot setup wizard with AI backend selection
- [ ] WeirdStudio v1.0 — full 5-panel web UI

### Phase 3 — Platform (Month 6 → Month 12)

Community, cloud services, content.

- [ ] WeirdConnect Relay — internet CV routing, remote collaboration
- [ ] Patch Library Cloud Sync
- [ ] Ideas Feed AI curation — server-side + community ratings
- [ ] WeirdConnect SDK — open release, third-party developer docs
- [ ] WeirdPhone Android
- [ ] OTA firmware system for WeirdOS
- [ ] WeirdBox v2 PCB design (custom carrier board, target $165 COGS)

### Phase 4 — Expansion (Year 2)

- [ ] WeirdFace Eurorack module
- [ ] WeirdSynths Academy (course content)
- [ ] Community Patch Store
- [ ] Expanded model zoo (musical genre classifiers, neural rhythm models)
- [ ] VST3 / AU plugin (WeirdSynths outside VCV Rack)

---

## 11. WeirdOS Architecture (Updated for AI HAT+ 2)

```
WeirdOS (Buildroot Linux, ARM64)
│
├── Kernel: Linux 6.6, Hailo-10H PCIe driver (hailort)
├── Camera: libcamera → GStreamer pipeline
│
├── hailort daemon ─────────────────────────────────────────┐
│   ├── Tracking Context (real-time, ~14 TOPS)              │
│   │   ├── face_landmarks.hef                              │
│   │   ├── phoneme_detector.hef                            │
│   │   ├── emotion_classifier.hef                          │
│   │   ├── body_pose.hef                                   │
│   │   ├── hand_landmarks.hef                              │
│   │   ├── gaze_estimator.hef                              │
│   │   └── depth_estimator.hef                             │
│   │                                                       │
│   └── Batch Context (on-demand, ~24 TOPS available)       │
│       ├── qwen2.5_coder_1.5b.hef  (LLM)                  │
│       ├── ddsp_instrument.hef     (TIMBRE)                │
│       ├── demucs_lite.hef         (SPLIT)                 │
│       ├── hifigan_lite.hef        (FORGE)                 │
│       └── amp_model.hef           (IRON)                  │
│                                                         8GB│
│                                                   LPDDR4X  │
└─────────────────────────────────────────────────────────────┘
│
├── bridge/ (Python)
│   ├── tracking_bridge.py  → UDP multicast to VCV modules
│   ├── llm_server.py       → FastAPI endpoint for WeirdMaker
│   ├── synthesis_api.py    → REST endpoints for synthesis AI
│   └── discovery.py        → WeirdConnect device advertisement
│
├── WeirdStudio (Node.js, served at :80)
│   ├── Dashboard
│   ├── WeirdMaker
│   ├── Ideas Feed
│   ├── Patch Library
│   └── Settings
│
└── System services
    ├── weirdos-bridge.service    (tracking → VCV UDP)
    ├── weirdhailort.service      (HAT management)
    ├── weirdstudio.service       (web UI)
    ├── weirdllm.service          (LLM inference daemon)
    └── weirdconnect.service      (discovery + relay)
```

---

## 12. Guiding Principles

1. **Offline-first.** Every core feature works without internet. Network is enhancement, not requirement.

2. **Privacy by default.** Local AI backend is recommended at setup. No data leaves the device unless the user explicitly enables it.

3. **Body as instrument.** The tracking is not a gimmick. Every module is designed around real musical expression — what your face, hands, and voice actually do when you perform.

4. **Modular philosophy.** The platform is modular. WeirdPhone works without WeirdBox. WeirdBox works without WeirdPhone. WeirdFace works standalone. Mix and match.

5. **Open protocol.** WeirdConnect is documented and open. Third-party developers can build devices that speak it. This makes the ecosystem larger than WeirdSynths alone.

6. **Maker culture.** WeirdMaker ships with the device because the people who buy WeirdBox are also the people who want to build their own modules. The line between user and creator is blurred intentionally.

7. **Hardware as distribution.** The physical box gets WeirdSynths onto desks and into studios. Software revenue sustains the business. The box itself doesn't need high margin at launch.

---

*Last updated: February 2026*
*See also: `weirdphone/docs/SPEC.md`, `weirdphone/docs/PROTOCOL.md`, `weirdos/`, `docs/`*
