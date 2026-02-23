//
//  ARKitManager.swift
//  WeirdPhone
//
//  Handles Face Tracking via ARKit with continuous blendshape emission.
//  Uses iPhone's Neural Engine for real-time processing at 60fps.
//  
//  ARKit Face Tracking provides 52 predefined blendshapes corresponding to
//  facial muscles and expressions. These are normalized 0.0–1.0 values.
//  
//  Canonical ARKit Blendshape Names (52 total):
//  - Eye: eyeBlinkLeft, eyeBlinkRight, eyeLookDownLeft, eyeLookDownRight, etc.
//  - Mouth: mouthSmileLeft, mouthSmileRight, mouthOpen, mouthPucker, mouthWide, etc.
//  - Jaw: jawOpen, jawForward, jawLeft, jawRight
//  - Cheek: cheekPuff, cheekSquintLeft, cheekSquintRight
//  - Nose: noseSneerLeft, noseSneerRight
//  - Tongue: tongueOut, tongueUp, tongueDown, tongueLeft, tongueRight
//  - Brow: browDownLeft, browDownRight, browInnerUp, browOuterUpLeft, browOuterUpRight
//  
//  Head pose (6DOF): pitch, yaw, roll + translation (tx, ty, tz)
//  Eye gaze: left/right look direction vectors (normalized)

import ARKit
import Combine
import SceneKit

class ARKitManager: NSObject, ObservableObject, ARSCNViewDelegate, ARSessionDelegate {
    // MARK: - Published State
    @Published var blendshapes: [String: Float] = [:]
    @Published var headPose: simd_float4x4 = matrix_identity_float4x4
    @Published var eyeLeftGaze: SIMD3<Float> = .zero
    @Published var eyeRightGaze: SIMD3<Float> = .zero
    @Published var isTracking: Bool = false
    
    // MARK: - ARKit Session
    private let arSceneView = ARSCNView()
    private let arSession = ARSession()
    private var isSessionRunning = false
    
    override init() {
        super.init()
        arSceneView.delegate = self
        arSession.delegate = self
    }
    
    // MARK: - Tracking Control
    func startTracking() {
        guard !isSessionRunning else { return }
        
        let configuration = ARFaceTrackingConfiguration()
        configuration.isLightEstimationEnabled = false /// Disable for lower latency
        configuration.providesAudioData = false
        
        arSession.run(configuration)
        isSessionRunning = true
    }
    
    func stopTracking() {
        guard isSessionRunning else { return }
        arSession.pause()
        isSessionRunning = false
        isTracking = false
    }
    
    // MARK: - ARSessionDelegate (Blendshape Updates)
    func session(_ session: ARSession, didUpdate anchors: [AnchorProtocol]) {
        guard let faceAnchor = anchors.first as? ARFaceAnchor else { return }
        
        /// Parse 52 ARKit blendshapes into dictionary
        var shapes: [String: Float] = [:]
        for (name, value) in faceAnchor.blendShapes {
            shapes[name.rawValue] = value as? Float ?? 0.0
        }
        
        DispatchQueue.main.async {
            self.blendshapes = shapes
            self.headPose = faceAnchor.transform
            self.eyeLeftGaze = SIMD3(0, 0, 1) /// TODO: extract from ARBlendShapeLocation
            self.eyeRightGaze = SIMD3(0, 0, 1)
            self.isTracking = true
        }
    }
    
    func session(_ session: ARSession, didFailWithError error: Error) {
        DispatchQueue.main.async {
            self.isTracking = false
        }
    }
    
    // MARK: - ARSCNViewDelegate
    func renderer(_ renderer: SCNSceneRenderer, didUpdate node: SCNNode, for anchor: AnchorProtocol) {
        /// Callback when face geometry updates (if using 3D mesh overlay)
        /// Can be used for additional processing of blendshapes
    }
}
