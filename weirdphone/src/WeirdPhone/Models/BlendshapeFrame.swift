//
//  BlendshapeFrame.swift
//  WeirdPhone
//
//  Snapshot of all 52 ARKit blendshapes plus head pose and eye gaze.
//  Convertible to WeirdConnect v3 binary packet format.
//  
//  Blendshape Categories (52 total):
//  Eyes (12): eyeBlink, eyeLookDown, eyeLookIn, eyeLookOut, eyeLookUp
//  Mouth (20): mouthClose, mouthDimple, mouthFrown, mouthFunnel, mouthLeft/Right,
//              mouthOpen, mouthPucker, mouthRollLower/Upper, mouthShrugLower/Upper,
//              mouthSmile, mouthStretch, mouthWide
//  Jaw (4): jawOpen, jawForward, jawLeft, jawRight
//  Cheek (3): cheekPuff, cheekSquint
//  Nose (2): noseSneer
//  Tongue (6): tongueOut, (others TBD)
//  Brow (5): browDown, browInnerUp, browOuterUp
//  
//  Head Pose (6DOF): pitch, yaw, roll, tx, ty, tz
//  Eye Gaze: leftLookX/Y, rightLookX/Y (normalized vectors)
//  
//  Packet Format (260 bytes, little-endian):
//  [Magic: 0x57 0x52 0x44 0x03][FrameID][Timestamp][64 CVs][Padding]

import Foundation

struct BlendshapeFrame {
    // MARK: - Eye Blendshapes (12)
    var eyeBlinkLeft: Float = 0
    var eyeBlinkRight: Float = 0
    var eyeLookDownLeft: Float = 0
    var eyeLookDownRight: Float = 0
    var eyeLookInLeft: Float = 0
    var eyeLookInRight: Float = 0
    var eyeLookOutLeft: Float = 0
    var eyeLookOutRight: Float = 0
    var eyeLookUpLeft: Float = 0
    var eyeLookUpRight: Float = 0
    var eyeWideLeft: Float = 0
    var eyeWideRight: Float = 0
    
    // MARK: - Mouth Blendshapes (20)
    var mouthClose: Float = 0
    var mouthDimpleLeft: Float = 0
    var mouthDimpleRight: Float = 0
    var mouthFrownLeft: Float = 0
    var mouthFrownRight: Float = 0
    var mouthFunnel: Float = 0
    var mouthLeft: Float = 0
    var mouthOpen: Float = 0
    var mouthPucker: Float = 0
    var mouthRight: Float = 0
    var mouthRollLower: Float = 0
    var mouthRollUpper: Float = 0
    var mouthShrugLower: Float = 0
    var mouthShrugUpper: Float = 0
    var mouthSmileLeft: Float = 0
    var mouthSmileRight: Float = 0
    var mouthStretchLeft: Float = 0
    var mouthStretchRight: Float = 0
    var mouthUpperUpLeft: Float = 0
    var mouthUpperUpRight: Float = 0
    var mouthWide: Float = 0
    
    // MARK: - Jaw (4)
    var jawOpen: Float = 0
    var jawForward: Float = 0
    var jawLeft: Float = 0
    var jawRight: Float = 0
    
    // MARK: - Cheek & Nose (5)
    var cheekPuffLeft: Float = 0
    var cheekPuffRight: Float = 0
    var cheekSquintLeft: Float = 0
    var cheekSquintRight: Float = 0
    var noseSneerLeft: Float = 0
    var noseSneerRight: Float = 0
    
    // MARK: - Tongue (6)
    var tongueOut: Float = 0
    var tongueUp: Float = 0
    var tongueDown: Float = 0
    var tongueLeft: Float = 0
    var tongueRight: Float = 0
    var tongueCurl: Float = 0
    
    // MARK: - Brow (5)
    var browDownLeft: Float = 0
    var browDownRight: Float = 0
    var browInnerUp: Float = 0
    var browOuterUpLeft: Float = 0
    var browOuterUpRight: Float = 0
    
    // MARK: - Head Pose (6DOF)
    var headPitch: Float = 0 /// Rotation X
    var headYaw: Float = 0   /// Rotation Y
    var headRoll: Float = 0  /// Rotation Z
    var headTx: Float = 0    /// Translation X
    var headTy: Float = 0    /// Translation Y
    var headTz: Float = 0    /// Translation Z
    
    // MARK: - Eye Gaze
    var eyeLeftLookX: Float = 0
    var eyeLeftLookY: Float = 0
    var eyeRightLookX: Float = 0
    var eyeRightLookY: Float = 0
    
    // MARK: - Serialization
    func toPacketData() -> Data {
        /// WeirdConnect v3 packet format (260 bytes)
        var data = Data(capacity: 260)
        
        /// Magic bytes: 0x57 0x52 0x44 0x03 ("WRD" + version)
        data.append(contentsOf: [0x57, 0x52, 0x44, 0x03])
        
        /// FrameID (4 bytes, little-endian)
        var frameId: UInt32 = UInt32(Date().timeIntervalSince1970 * 1000) & 0xFFFFFFFF
        data.append(contentsOf: frameId.littleEndianBytes)
        
        /// Timestamp (8 bytes, little-endian, milliseconds)
        var timestamp = UInt64(Date().timeIntervalSince1970 * 1000)
        data.append(contentsOf: timestamp.littleEndianBytes)
        
        /// Blendshapes and head pose as 32-bit floats
        /// TODO: Pack 52 blendshapes + 6 head pose values into payload
        /// Placeholder: append 64 zero floats
        for _ in 0..<64 {
            let floatBytes = Float(0).littleEndianBytes
            data.append(contentsOf: floatBytes)
        }
        
        /// Pad to 260 bytes
        while data.count < 260 {
            data.append(0)
        }
        
        return data
    }
}

// MARK: - Helper Extensions for Endianness
extension UInt32 {
    var littleEndianBytes: [UInt8] {
        return [
            UInt8((self >> 0) & 0xFF),
            UInt8((self >> 8) & 0xFF),
            UInt8((self >> 16) & 0xFF),
            UInt8((self >> 24) & 0xFF)
        ]
    }
}

extension UInt64 {
    var littleEndianBytes: [UInt8] {
        return [
            UInt8((self >> 0) & 0xFF),
            UInt8((self >> 8) & 0xFF),
            UInt8((self >> 16) & 0xFF),
            UInt8((self >> 24) & 0xFF),
            UInt8((self >> 32) & 0xFF),
            UInt8((self >> 40) & 0xFF),
            UInt8((self >> 48) & 0xFF),
            UInt8((self >> 56) & 0xFF)
        ]
    }
}

extension Float {
    var littleEndianBytes: [UInt8] {
        var value = self
        return withUnsafeBytes(of: &value) { Array($0) }
    }
}
