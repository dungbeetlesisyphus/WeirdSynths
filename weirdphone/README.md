# WeirdPhone

iOS app that turns any iPhone (X or later) into a biometric CV controller for VCV Rack, DAWs, and OSC/MIDI software. Uses Apple's TrueDepth camera + Neural Engine for 52 ARKit face blendshapes at 60fps — zero CPU cost.

**No Pi. No extra hardware. Just your phone.**

## Folder Structure

```
weirdphone/
├── docs/
│   ├── SPEC.md          Full product specification
│   └── PROTOCOL.md      WeirdConnect v3 binary protocol spec
├── wireframes/
│   └── index.html       Interactive screen mockups (open in browser)
├── src/WeirdPhone/
│   ├── WeirdPhoneApp.swift
│   ├── AppState.swift
│   ├── ARKitManager.swift
│   ├── StreamingManager.swift
│   ├── MappingManager.swift
│   ├── Models/
│   │   ├── ConnectionPreset.swift
│   │   └── BlendshapeFrame.swift
│   └── Views/
│       ├── MainView.swift
│       ├── TrackView.swift
│       └── ConnectView.swift
└── README.md            ← you are here
```

## Quick Start (Developer)

1. Open `src/WeirdPhone/` in Xcode 15+
2. Set your Team ID in Signing & Capabilities
3. Add `NSCameraUsageDescription` to Info.plist
4. Enable `Background Modes → Audio` for background streaming
5. Build to iPhone X or later (TrueDepth required)
6. In VCV Rack: load NERVE module, it auto-discovers WeirdPhone on same WiFi

## Protocol

WeirdPhone speaks **WeirdConnect v3** — a 260-byte UDP packet at up to 60fps.
Magic header: `0x57 0x52 0x44 0x03` ("WRD" + version 3)
Default port: `9000` (same as WeirdOS bridge — NERVE just works)

Full spec: `docs/PROTOCOL.md`

## Tiers

| | Free | Pro ($9.99) |
|---|---|---|
| CVs | 8 | 64 |
| Protocols | UDP | UDP + OSC + MIDI |
| Mapping | Fixed | Custom MAP screen |
| Presets | 1 | Unlimited |
| Background | — | ✓ |

## Platform

- iOS 17+
- iPhone X or later (TrueDepth camera)
- ARKit + Neural Engine (0% CPU, 60fps)
- SwiftUI + Combine architecture
