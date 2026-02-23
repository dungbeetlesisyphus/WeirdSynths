//
//  ConnectionPreset.swift
//  WeirdPhone
//
//  Codable preset for storing connection configuration.
//  Users can save/load multiple presets for different synths or hosts.
//  
//  Properties:
//  - name: Display name (e.g., "Eurorack via WiFi")
//  - protocolType: UDP, OSC, or MIDI
//  - host: IP address or hostname (e.g., "192.168.1.100")
//  - port: Network port (UDP: 9000–9999, OSC: typically 9000, MIDI: N/A)
//  - frameRateHz: Target streaming frame rate (60, 30, or 15)
//  - mappingId: References a MappingProfile (free tier ignores this)

import Foundation

struct ConnectionPreset: Identifiable, Codable {
    let id: UUID
    var name: String
    var protocolType: ProtocolType
    var host: String
    var port: UInt16
    var frameRateHz: Int
    var mappingId: UUID?
    
    enum ProtocolType: String, Codable {
        case udp = "UDP"
        case osc = "OSC"
        case midi = "MIDI"
    }
    
    init(id: UUID = UUID(), name: String, protocolType: ProtocolType,
         host: String, port: UInt16, frameRateHz: Int = 60, mappingId: UUID? = nil) {
        self.id = id
        self.name = name
        self.protocolType = protocolType
        self.host = host
        self.port = port
        self.frameRateHz = frameRateHz
        self.mappingId = mappingId
    }
    
    // MARK: - Default Presets
    static let defaults: [ConnectionPreset] = [
        ConnectionPreset(
            name: "Local Eurorack (UDP)",
            protocolType: .udp,
            host: "192.168.1.50",
            port: 9000,
            frameRateHz: 60
        ),
        ConnectionPreset(
            name: "TouchDesigner OSC",
            protocolType: .osc,
            host: "192.168.1.100",
            port: 9001,
            frameRateHz: 30
        ),
        ConnectionPreset(
            name: "iPad MIDI",
            protocolType: .midi,
            host: "127.0.0.1",
            port: 5004,
            frameRateHz: 60
        )
    ]
}
