//
//  StreamingManager.swift
//  WeirdPhone
//
//  Manages network streaming of blendshape data to external devices.
//  Supports three protocols: UDP, OSC, and MIDI.
//  
//  Protocol Details:
//  - UDP: Custom WeirdConnect v3 format (260-byte packets, 60fps target)
//  - OSC: Open Sound Control over UDP (address: /weirdphone/cv)
//  - MIDI: Control Change messages over USB MIDI or Network MIDI
//  
//  Packet Structure (WeirdConnect v3, 260 bytes):
//  [Magic: 4] [FrameID: 4] [Timestamp: 8] [64 CVs: 4 bytes each] [Padding: variable]
//  Magic bytes: 0x57 0x52 0x44 0x03 ("WRD" + version)
//  Endianness: little-endian for all multi-byte values
//  
//  Latency tracking via round-trip timestamps. Packets/sec counter updated per second.

import Combine
import Network

class StreamingManager: NSObject, ObservableObject {
    // MARK: - Published State
    @Published var isConnected: Bool = false
    @Published var latencyMs: Double = 0.0
    @Published var packetsPerSec: Int = 0
    
    // MARK: - Private State
    private var activeProtocol: StreamingProtocol?
    private var udpStreamer: UDPStreamer?
    private var frameCounter: UInt32 = 0
    private var packetsThisSecond: Int = 0
    private var lastCounterReset: Date = Date()
    
    // MARK: - Connection Management
    func connect(to preset: ConnectionPreset) {
        disconnect()
        
        switch preset.protocolType {
        case .udp:
            udpStreamer = UDPStreamer(host: preset.host, port: preset.port)
            udpStreamer?.connect()
            activeProtocol = .udp
            
        case .osc:
            /// TODO: Instantiate OSC sender
            activeProtocol = .osc
            
        case .midi:
            /// TODO: Instantiate MIDI sender
            activeProtocol = .midi
        }
        
        DispatchQueue.main.async {
            self.isConnected = true
        }
    }
    
    func disconnect() {
        udpStreamer?.disconnect()
        activeProtocol = nil
        
        DispatchQueue.main.async {
            self.isConnected = false
            self.latencyMs = 0.0
            self.packetsPerSec = 0
        }
    }
    
    // MARK: - Frame Streaming
    func send(_ frame: BlendshapeFrame) {
        guard isConnected else { return }
        
        switch activeProtocol {
        case .udp:
            let packet = frame.toPacketData()
            udpStreamer?.sendPacket(packet)
            
        case .osc:
            /// TODO: Send OSC message
            break
            
        case .midi:
            /// TODO: Send MIDI CC messages
            break
            
        case nil:
            break
        }
        
        /// Update packet counter
        packetsThisSecond += 1
        let now = Date()
        if now.timeIntervalSince(lastCounterReset) > 1.0 {
            DispatchQueue.main.async {
                self.packetsPerSec = self.packetsThisSecond
            }
            packetsThisSecond = 0
            lastCounterReset = now
        }
    }
}

// MARK: - UDP Streamer Implementation
private class UDPStreamer {
    private let host: String
    private let port: UInt16
    /// TODO: Add GCDAsyncUdpSocket property
    
    init(host: String, port: UInt16) {
        self.host = host
        self.port = port
    }
    
    func connect() {
        /// TODO: Initialize socket and connect to host:port
    }
    
    func disconnect() {
        /// TODO: Close socket
    }
    
    func sendPacket(_ data: Data) {
        /// TODO: Send data via socket
    }
    
    private func buildPacket(_ frame: BlendshapeFrame) -> Data {
        /// WeirdConnect v3 binary packet format
        /// Magic: 0x57 0x52 0x44 0x03
        /// FrameID: 4 bytes, little-endian
        /// Timestamp: 8 bytes, little-endian (milliseconds since epoch)
        /// CVs: 64 × 4-byte floats, little-endian
        /// Padding: remainder to 260 bytes
        var data = Data(capacity: 260)
        data.append(contentsOf: [0x57, 0x52, 0x44, 0x03]) /// Magic
        /// TODO: Append frameID, timestamp, CV values
        return data
    }
}

// MARK: - Protocol Enum
private enum StreamingProtocol {
    case udp
    case osc
    case midi
}
