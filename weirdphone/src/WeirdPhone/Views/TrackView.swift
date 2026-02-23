//
//  TrackView.swift
//  WeirdPhone
//
//  Live face tracking visualization with ARSCNView and control voltage meters.
//  Displays:
//  - 3D face model with landmark overlay (from ARKit)
//  - Scrollable list of CV meter bars (64 possible outputs, limited by tier)
//  - Connection status badge
//  - Real-time streaming stats (latency, packets/sec)
//  
//  Free Tier: Shows 8 CV meters
//  Pro Tier: Shows all 64 CV meters
//  
//  AR Preview: Renders face landmarks and 52 blendshapes in real-time

import SwiftUI

struct TrackView: View {
    @EnvironmentObject var appState: AppState
    
    /// Determine visible meter count based on tier
    var visibleMeterCount: Int {
        appState.isPro ? 64 : 8
    }
    
    var body: some View {
        ZStack {
            /// Background
            Color.black
                .ignoresSafeArea()
            
            VStack(spacing: 12) {
                // MARK: - Header with Status
                HStack {
                    VStack(alignment: .leading, spacing: 4) {
                        Text("Live Tracking")
                            .font(.headline)
                            .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
                        
                        HStack(spacing: 8) {
                            Circle()
                                .fill(appState.isConnected ? Color.green : Color.red)
                                .frame(width: 8, height: 8)
                            
                            Text(appState.isConnected ? "Connected" : "Disconnected")
                                .font(.caption)
                                .foregroundColor(.gray)
                        }
                    }
                    
                    Spacer()
                    
                    VStack(alignment: .trailing, spacing: 4) {
                        Text("\(appState.packetsPerSec) pps")
                            .font(.caption2)
                            .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
                        
                        Text("\(String(format: "%.1f", appState.latencyMs))ms")
                            .font(.caption2)
                            .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
                    }
                }
                .padding(12)
                .background(Color(white: 0.1))
                .cornerRadius(6)
                
                // MARK: - AR Preview (placeholder)
                RoundedRectangle(cornerRadius: 8)
                    .fill(Color(white: 0.05))
                    .overlay(
                        RoundedRectangle(cornerRadius: 8)
                            .stroke(Color(red: 0, green: 1, blue: 0.533), lineWidth: 1)
                    )
                    .frame(height: 200)
                    .overlay(
                        Text("AR Face Preview")
                            .foregroundColor(.gray)
                    )
                
                // MARK: - CV Meters Scroll
                Text("Control Voltage Outputs")
                    .font(.caption)
                    .foregroundColor(.gray)
                    .frame(maxWidth: .infinity, alignment: .leading)
                
                ScrollView {
                    VStack(spacing: 8) {
                        ForEach(0..<visibleMeterCount, id: \.self) { index in
                            CVMeterRow(
                                index: index,
                                value: appState.outputValues.count > index ?
                                    appState.outputValues[index] : 0
                            )
                        }
                    }
                }
                
                Spacer()
            }
            .padding(16)
        }
    }
}

// MARK: - CV Meter Component
struct CVMeterRow: View {
    let index: Int
    let value: Float
    
    var body: some View {
        HStack(spacing: 8) {
            Text("CV\(index)")
                .font(.caption)
                .foregroundColor(.gray)
                .frame(width: 40, alignment: .leading)
            
            /// Progress bar (0–10V scale)
            GeometryReader { geometry in
                ZStack(alignment: .leading) {
                    RoundedRectangle(cornerRadius: 3)
                        .fill(Color(white: 0.1))
                    
                    RoundedRectangle(cornerRadius: 3)
                        .fill(Color(red: 0, green: 1, blue: 0.533))
                        .frame(width: geometry.size.width * CGFloat(min(value / 10.0, 1.0)))
                }
            }
            .frame(height: 12)
            
            Text(String(format: "%.2fV", value))
                .font(.caption2)
                .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
                .frame(width: 50, alignment: .trailing)
        }
    }
}

// MARK: - Preview
#if DEBUG
struct TrackView_Previews: PreviewProvider {
    static var previews: some View {
        TrackView()
            .environmentObject(AppState.preview)
            .preferredColorScheme(.dark)
    }
}
#endif
