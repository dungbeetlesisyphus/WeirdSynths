//
//  WeirdPhoneApp.swift
//  WeirdPhone
//
//  Main entry point for the WeirdPhone iOS application.
//  Architecture Overview:
//  - AppState: Reactive singleton combining ARKitManager, StreamingManager, MappingManager
//  - View hierarchy: TabView -> (TrackView, ConnectView, MapView, PresetsView, SettingsView)
//  - State flows: ARKit blendshapes → AppState → Streaming → Network
//  - Dependencies: ARKit (face tracking), GCDAsyncSocket (UDP), CoreMIDI, OSC libraries
//
//  The app runs at 60fps when face tracking is active. All Codable models support
//  serialization for presets. Environment injection enables preview support and testing.

import SwiftUI

@main
struct WeirdPhoneApp: App {
    // MARK: - State Management
    /// Injected as environment object for all views to access global state
    @StateObject private var appState = AppState()
    
    // MARK: - App Lifecycle
    var body: some Scene {
        WindowGroup {
            MainView()
                .environmentObject(appState)
                /// Dark mode forced for CRT phosphor aesthetic
                .preferredColorScheme(.dark)
        }
    }
}

// MARK: - Preview Support
#if DEBUG
struct WeirdPhoneApp_Previews: PreviewProvider {
    static var previews: some View {
        MainView()
            .environmentObject(AppState.preview)
            .preferredColorScheme(.dark)
    }
}

extension AppState {
    /// Mock AppState for SwiftUI previews and testing
    static let preview = AppState()
}
#endif
