//
//  MainView.swift
//  WeirdPhone
//
//  Main TabView container with 5 tabs for tracking, streaming, mapping, presets, and settings.
//  CRT phosphor color scheme: bright neon green (#00FF88) on dark background.
//  All navigation via SF Symbols and tab bar.

import SwiftUI

struct MainView: View {
    @EnvironmentObject var appState: AppState
    @State private var selectedTab: Int = 0
    
    var body: some View {
        TabView(selection: $selectedTab) {
            // MARK: - Track Tab
            TrackView()
                .tabItem {
                    Label("Track", systemImage: "face.smiling")
                }
                .tag(0)
            
            // MARK: - Connect Tab
            ConnectView()
                .tabItem {
                    Label("Connect", systemImage: "antenna.radiowaves.left.and.right")
                }
                .tag(1)
            
            // MARK: - Map Tab
            MapView()
                .tabItem {
                    Label("Map", systemImage: "slider.horizontal.3")
                }
                .tag(2)
            
            // MARK: - Presets Tab
            PresetsView()
                .tabItem {
                    Label("Presets", systemImage: "square.grid.2x2")
                }
                .tag(3)
            
            // MARK: - Settings Tab
            SettingsView()
                .tabItem {
                    Label("Settings", systemImage: "gear")
                }
                .tag(4)
        }
        .onAppear {
            /// Apply CRT phosphor color scheme
            let appearance = UITabBarAppearance()
            appearance.backgroundColor = UIColor(red: 0.05, green: 0.05, blue: 0.05, alpha: 1.0)
            UITabBar.appearance().standardAppearance = appearance
            
            /// Start ARKit tracking
            appState.startTracking()
        }
        .onDisappear {
            appState.stopTracking()
        }
    }
}

// MARK: - Placeholder Views (stubs)
struct MapView: View {
    var body: some View {
        VStack {
            Text("Mapping Configuration")
                .font(.headline)
                .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color.black)
    }
}

struct PresetsView: View {
    var body: some View {
        VStack {
            Text("Connection Presets")
                .font(.headline)
                .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color.black)
    }
}

struct SettingsView: View {
    var body: some View {
        VStack {
            Text("Settings")
                .font(.headline)
                .foregroundColor(Color(red: 0, green: 1, blue: 0.533))
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(Color.black)
    }
}

// MARK: - Preview
#if DEBUG
struct MainView_Previews: PreviewProvider {
    static var previews: some View {
        MainView()
            .environmentObject(AppState.preview)
            .preferredColorScheme(.dark)
    }
}
#endif
