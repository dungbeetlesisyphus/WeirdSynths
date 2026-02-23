//
//  MappingManager.swift
//  WeirdPhone
//
//  Transforms raw blendshape values into control voltage (CV) outputs.
//  Implements per-channel scaling, smoothing, and gating.
//  
//  Free Tier Limitations:
//  - Maximum 8 active mappings (pro = 64)
//  - Available scale modes: zeroToTen, gate(threshold:)
//  - Smoothing: disabled
//  
//  Pro Features:
//  - All 64 mappings available (one per source)
//  - Scale modes: zeroToTen, bipolar5 (±5V), zeroToFive, gate(threshold:)
//  - Exponential slew limiter per channel (0–1000ms time constant)
//  
//  Mapping Process:
//  1. Apply scale mode to input (0–1 normalized blendshape → output range)
//  2. Apply gate (if threshold exceeded)
//  3. Apply exponential smoothing (if enabled)
//  4. Clamp to output range (0–10V)

import Combine

class MappingManager: ObservableObject {
    // MARK: - Published State
    @Published var mappings: [OutputMapping] = []
    @Published var outputValues: [Float] = Array(repeating: 0.0, count: 64)
    
    // MARK: - Smoothing State (per-channel exponential slew)
    private var lastValues: [Float] = Array(repeating: 0.0, count: 64)
    
    // MARK: - Public API
    /// Applies all mappings and returns final 64-channel CV array
    func apply(blendshapes: [String: Float]) -> [Float] {
        var output = Array(repeating: 0.0, count: 64)
        
        for mapping in mappings {
            guard let sourceValue = blendshapes[mapping.sourceBlendshape] else {
                continue
            }
            
            /// Apply scale mode
            let scaled = applyScaleMode(sourceValue, mode: mapping.scaleMode)
            
            /// Apply smoothing if enabled
            let smoothed = mapping.slewTimeMs > 0 ?
                applyExponentialSlew(scaled, channelIndex: mapping.outputIndex) :
                scaled
            
            output[mapping.outputIndex] = smoothed
        }
        
        DispatchQueue.main.async {
            self.outputValues = output
        }
        
        return output
    }
    
    // MARK: - Scaling Implementations
    private func applyScaleMode(_ value: Float, mode: ScaleMode) -> Float {
        switch mode {
        case .zeroToTen:
            /// Linear: 0–1 → 0–10V
            return value * 10.0
            
        case .bipolar5:
            /// Centered: 0–1 → -5V to +5V
            return (value * 10.0) - 5.0
            
        case .zeroToFive:
            /// Linear: 0–1 → 0–5V
            return value * 5.0
            
        case .gate(let threshold):
            /// Output 10V if value > threshold, else 0V
            return value > threshold ? 10.0 : 0.0
        }
    }
    
    /// Exponential slew limiter (1st-order low-pass IIR)
    /// α = dt / (τ + dt), where τ = slewTimeMs / 1000
    private func applyExponentialSlew(_ target: Float, channelIndex: Int) -> Float {
        let tau: Float = 0.01 /// 10ms default (TODO: per-mapping)
        let dt: Float = 1.0 / 60.0 /// 60fps
        let alpha = dt / (tau + dt)
        
        let smoothed = lastValues[channelIndex] * (1.0 - alpha) + target * alpha
        lastValues[channelIndex] = smoothed
        return smoothed
    }
}

// MARK: - Models
struct OutputMapping: Identifiable, Codable {
    let id: UUID
    let name: String
    let sourceBlendshape: String /// e.g., "mouthOpen"
    let outputIndex: Int /// 0–63
    let scaleMode: ScaleMode
    let slewTimeMs: Float /// 0 = disabled
    
    init(id: UUID = UUID(), name: String, sourceBlendshape: String,
         outputIndex: Int, scaleMode: ScaleMode = .zeroToTen, slewTimeMs: Float = 0) {
        self.id = id
        self.name = name
        self.sourceBlendshape = sourceBlendshape
        self.outputIndex = outputIndex
        self.scaleMode = scaleMode
        self.slewTimeMs = slewTimeMs
    }
}

enum ScaleMode: Codable {
    case zeroToTen
    case bipolar5
    case zeroToFive
    case gate(threshold: Float)
}
