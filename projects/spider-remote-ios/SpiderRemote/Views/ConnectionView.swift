import SwiftUI

struct ConnectionView: View {
    @ObservedObject var ble: SpiderBLEManager

    var body: some View {
        VStack(spacing: 16) {
            statusHeader

            if ble.state == .connected {
                connectedPanel
            } else {
                scanPanel
            }

            if let error = ble.lastError {
                Text(error)
                    .font(.footnote)
                    .foregroundStyle(.red)
                    .multilineTextAlignment(.center)
            }

            Spacer()
        }
        .padding()
        .navigationTitle("连接")
    }

    private var statusHeader: some View {
        HStack {
            Text(SpiderBLEConstants.deviceName)
                .font(.headline)
            Spacer()
            Circle()
                .fill(statusColor)
                .frame(width: 10, height: 10)
            Text(statusText)
                .font(.subheadline)
                .foregroundStyle(.secondary)
        }
        .padding()
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
    }

    private var connectedPanel: some View {
        VStack(spacing: 12) {
            Text("已连接 \(ble.connectedPeripheralName ?? SpiderBLEConstants.deviceName)")
                .font(.subheadline)
            Button("断开连接") {
                ble.disconnect()
            }
            .buttonStyle(.borderedProminent)
            .tint(.red)
        }
    }

    private var scanPanel: some View {
        VStack(spacing: 12) {
            HStack {
                Button(ble.state == .scanning ? "停止扫描" : "扫描 SpiderBod") {
                    if ble.state == .scanning {
                        ble.stopScan()
                    } else {
                        ble.startScan()
                    }
                }
                .buttonStyle(.borderedProminent)
                .disabled(ble.state == .poweredOff)
            }

            if ble.discoveredPeripherals.isEmpty {
                Text(ble.state == .scanning ? "正在扫描…" : "未发现设备")
                    .foregroundStyle(.secondary)
                    .padding(.top, 8)
            } else {
                List(ble.discoveredPeripherals, id: \.identifier) { peripheral in
                    Button {
                        ble.connect(peripheral)
                    } label: {
                        HStack {
                            Text(peripheral.name ?? SpiderBLEConstants.deviceName)
                            Spacer()
                            if ble.state == .connecting {
                                ProgressView()
                            }
                        }
                    }
                    .disabled(ble.state == .connecting)
                }
                .listStyle(.plain)
            }
        }
    }

    private var statusText: String {
        switch ble.state {
        case .poweredOff: return "蓝牙关闭"
        case .idle: return "就绪"
        case .scanning: return "扫描中"
        case .connecting: return "连接中"
        case .connected: return "已连接"
        case .disconnected: return "已断开"
        }
    }

    private var statusColor: Color {
        switch ble.state {
        case .connected: return .green
        case .connecting, .scanning: return .yellow
        case .poweredOff: return .gray
        default: return .orange
        }
    }
}
