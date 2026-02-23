//
//  AppState.swift
//  WeirdPhone
//
//  Central reactive data store combining three manager subsystems.
//  This follows the Coordinator pattern for SwiftUI apps.
//  
//  Reactive Data Flow:
//  1. ARKitManager publishes blendshapes/headPose at 60fps (Neural Engine)
//  2. AppState observes via @Published and combines into composite state
//  3. StreamingManager reads combined state and queues packets
//  4. Views subscribe to published properties for automatic UI updates
//  
//  Pro subscription (isPro) unlocks:
//  - All 64 control voltage mappings (free tier = 8)
//  - Custom scale modes and smoothing per channel
//  - MIDI and OSC protocol support (free = UDP only)

import SwiftUI
import Combine

class AppState: NSObject, ObservableObject {
    // MARK: - Published Properties (UI reactivity)
    @Published var isConnected: Bool = false
    @Published var isPro: Bool = false
    @Published var activePreset: ConnectionPreset?
    @Published var latencyMs: Double = 0.0
    @Published var packetsPerSec: Int = 0
    
    @Published var blendshapes: [String: Float] = [:]
    @Published var headPose: simd_float4x4 = matrix_identity_float4x4
    @Published var eyeLeftGaze: SIMD3<Float> = .zero
    @Published var eyeRightGaze: SIMD3<Float> = .zero
    
    // MARK: - Manager Instances
    private let arkitManager: ARKitManager
    private let streamingManager: StreamingManager
    private let mappingManager: MappingManager
    
    private var cancellables = Set<AnyCancellable>()
    
    // MARK: - Initialization
    override init() {
        self.arkitManager = ARKitManager()
        self.streamingManager = StreamingManager()
        self.mappingManager = MappingManager()
        super.init()
        setupBindings()
    }
    
    // MARK: - Reactive Bindings
    private func setupBindings() {
        /// Observe ARKit tracking updates
        arkitManager.$blendshapes
            .assign(to: &$blendshapes)
        
        arkitManager.$headPose
            .assign(to: &$headPose)
        
        arkitManager.$eyeLeftGaze
            .assign(to: &$eyeLeftGaze)
        
        arkitManager.$eyeRightGaze
            .assign(to: &$eyeRightGaze)
        
        /// Observe streaming connection state
        streamingManager.$isConnected
            .assign(to: &$isConnected)
        
        streamingManager.$latencyMs
            .assign(to: &$latencyMs)
        
        streamingManager.$packetsPerSec
            .assign(to: &$packetsPerSec)
    }
    
    // MARK: - Public API
    func startTracking() {
        arkitManager.startTracking()
    }
    
    func stopTracking() {
        arkitManager.stopTracking()
    }
    
    func connectToPreset(_ preset: ConnectionPreset) {
        activePreset = preset
        streamingManager.connect(to: preset)
    }
    
    func disconnect() {
        streamingManager.disconnect()
        activePreset = nil
    }
}
