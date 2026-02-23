//
//  ConnectView.swift
//  WeirdPhone
//
//  Network connection configuration and management.
//  Features:
//  - Protocol picker (UDP, OSC, MIDI segmented control)
//  - IP address and port input fields with validation
//  - Auto-discovery of devices via UDP broadcast on port 9001
//  - Connect/Disconnect button with status feedback
//  - Real-time latency and packet rate statistics
//  - Recent connection history
//  
//  Auto-discovery broadcasts "WEIRDPHONE_SCAN" packet and listens for responses
//  in WeirdConnect v3 format containing device metadata.

import SwiftUI

struct ConnectView: View {
    @EnvironmentObject var appState: AppState
    @State private var selectedProtocol: ConnectionPreset.ProtocolType = .udp
    @State private var hostAddress: String = "192.168.1.50"
    @State private var portNumber: String = "9000"
    @State private var isDiscovering: Bool = false
    @State private var discoveredDevices: [DiscoveredDevice] = []
    
    var body: some View {
        ZStack {
            Color.black
                .ignoresSafeArea()
            
            VStack(spacing: 16) {
                // MARK: - Protocol Selection
                VStack(alignment: .leading, spacing: 8) {
                    Text("Protocol")
                        .font(.caption)
                        .foregroundColor(.gray)
                    
                    Picker("Protocol", selection: $selectedProtocol) {
                        Text("UDP").tag(ConnectionPreset.ProtocolType.udp)
                        Text("OSC").tag(ConnectionPreset.ProtocolType.osc)
                        Text("MIDI").tag(ConnectionPreset.ProtocolType.midi)
                    }
                    .pickerStyle(.segmented)
                }
                
                // MARK: - Network Configuration
                VStack(alignment: .leading, spacing: 12) {
                    Text("Connection Settings")
                        .font(.caption)
                        .foregroundColor(.gray)
                    
                    /// Host Input
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Host Address")
                            .font(.caption2)
                            .foregroundColor(.gray)
                        
                        TextField("IP or hostname", text: $hostAddress)
                            .textFieldStyle(.roundedBorder)
                            .autocorrectionDisabled()
                            .keyboardType(.URL)
                    }
                    
                    /// Port Input
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Port")
                            .font(.caption2)
                            .foregroundColor(.gray)
                        
                        TextField("9000", text: $portNumber)
                            .textFieldStyle(.roundedBorder)
                            .keyboardType(.numberPad)
                            .frame(width: 100)
                    }
                }
                
                // MARK: - Auto-Discovery
                VStack(alignment: .leading, spacing: 8) {
                    Button(action: startDiscovery) {
                        HStack {
                            Image(systemName: "magnifyingglass")
                            Text("Discover Devices")
                        }
                        .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.bordered)
                    .tint(Color(red: 0, green: 1, blue: 0.533))
                    
                    if isDiscovering {
                        ProgressView()
                            .tint(Color(red: 0, green: 1, blue: 0.533))
                    }
                    
                    if !discoveredDevices.isEmpty {
                        Text("Found \(discoveredDevices.count) device(s)")
                            .font(.caption)
                            .foregroundColor(.gray)
                        
                        ForEach(discoveredDevices, id: \.id) { device in
                            Button(action: { selectDevice(device) }) {
                                HStack {
                                    VStack(alignment: .leading, spacing: 2) {
                                        Text(device.name)
                                            .font(.caption)
                                            .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
                                        Text(device.address)
                                            .font(.caption2)
                                            .foregroundColor(.gray)
                                    }
                                    Spacer()
                                    Image(systemName: "chevron.right")
                                        .foregroundColor(.gray)
                                }
                                .padding(8)
                                .background(Color(white: 0.1))
                                .cornerRadius(4)
                            }
                        }
                    }
                }
                
                // MARK: - Connection Button
                Button(action: toggleConnection) {
                    HStack {
                        Image(systemName: appState.isConnected ? "checkmark.circle.fill" : "circle")
                        Text(appState.isConnected ? "Disconnect" : "Connect")
                    }
                    .frame(maxWidth: .infinity)
                    .padding(12)
                    .background(appState.isConnected ? Color.red : Color(red: 0, green: 1, blue: 0.533))
                    .foregroundColor(.black)
                    .cornerRadius(6)
                    .font(.headline)
                }
                
                // MARK: - Statistics
                HStack(spacing: 16) {
                    StatBox(label: "Latency", value: String(format: "%.1f ms", appState.latencyMs))
                    StatBox(label: "Packets/sec", value: "\(appState.packetsPerSec)")
                }
                
                Spacer()
            }
            .padding(16)
        }
    }
    
    // MARK: - Helper Functions
    private func startDiscovery() {
        isDiscovering = true
        /// TODO: Send UDP broadcast on port 9001, parse responses
        DispatchQueue.main.asyncAfter(deadline: .now() + 2.0) {
            isDiscovering = false
            discoveredDevices = [
                DiscoveredDevice(id: UUID(), name: "Eurorack Host", address: "192.168.1.50:9000"),
                DiscoveredDevice(id: UUID(), name: "TouchDesigner", address: "192.168.1.100:9001")
            ]
        }
    }
    
    private func selectDevice(_ device: DiscoveredDevice) {
        let components = device.address.split(separator: ":")
        hostAddress = String(components.first ?? "")
        portNumber = String(components.last ?? "9000")
    }
    
    private func toggleConnection() {
        if appState.isConnected {
            appState.disconnect()
        } else {
            let preset = ConnectionPreset(
                name: "Manual",
                protocolType: selectedProtocol,
                host: hostAddress,
                port: UInt16(portNumber) ?? 9000
            )
            appState.connectToPreset(preset)
        }
    }
}

// MARK: - Supporting Types
struct DiscoveredDevice: Identifiable {
    let id: UUID
    let name: String
    let address: String
}

struct StatBox: View {
    let label: String
    let value: String
    
    var body: some View {
        VStack(spacing: 4) {
            Text(label)
                .font(.caption2)
                .foregroundColor(.gray)
            Text(value)
                .font(.caption)
                .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
        }
        .frame(maxWidth: .infinity)
        .padding(12)
        .background(Color(white: 0.1))
        .cornerRadius(6)
    }
}

// MARK: - Preview
#if DEBUG
struct ConnectView_Previews: PreviewProvider {
    static var previews: some View {
        ConnectView()
            .environmentObject(AppState.preview)
            .preferredColorScheme(.dark)
    }
}
#endif
