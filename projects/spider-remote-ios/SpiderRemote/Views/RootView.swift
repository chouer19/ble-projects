import SwiftUI

struct RootView: View {
    @StateObject private var ble = SpiderBLEManager()

    var body: some View {
        TabView {
            NavigationView {
                RemoteView(ble: ble)
            }
            .navigationViewStyle(.stack)
            .tabItem {
                Label("遥控", systemImage: "gamecontroller")
            }

            NavigationView {
                SpeedSettingsView(ble: ble)
            }
            .navigationViewStyle(.stack)
            .tabItem {
                Label("速度", systemImage: "speedometer")
            }

            NavigationView {
                ConnectionView(ble: ble)
            }
            .navigationViewStyle(.stack)
            .tabItem {
                Label("连接", systemImage: "antenna.radiowaves.left.and.right")
            }
        }
    }
}
