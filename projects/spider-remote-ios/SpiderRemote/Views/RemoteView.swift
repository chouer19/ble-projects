import SwiftUI

struct RemoteView: View {
    @ObservedObject var ble: SpiderBLEManager

    var body: some View {
        ScrollView {
            VStack(spacing: 16) {
                connectionBadge

                motionButton("Stand") {
                    ble.sendMotion(.stand)
                }

                HStack(spacing: 12) {
                    motionButton("Turn L") { ble.sendMotion(.turnLeft) }
                    stopButton
                    motionButton("Turn R") { ble.sendMotion(.turnRight) }
                }

                motionButton("Forward") {
                    ble.sendMotion(.forward)
                }

                motionButton("Backward") {
                    ble.sendMotion(.backward)
                }

                HStack(spacing: 12) {
                    motionButton("Spin L") { ble.sendMotion(.spinLeft) }
                    motionButton("Spin R") { ble.sendMotion(.spinRight) }
                }

                motionButton("Splay") {
                    ble.sendMotion(.splay)
                }

                Text("点按发送一条命令；循环动作需再点 Stop 或切换动作。")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                    .padding(.top, 8)
            }
            .padding()
        }
        .navigationTitle("遥控")
    }

    private var connectionBadge: some View {
        HStack {
            Text(SpiderBLEConstants.deviceName)
                .font(.headline)
            Spacer()
            Circle()
                .fill(ble.state == .connected ? Color.green : Color.orange)
                .frame(width: 10, height: 10)
            Text(ble.state == .connected ? "已连接" : "未连接")
                .font(.subheadline)
                .foregroundStyle(.secondary)
        }
        .padding()
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
    }

    private var stopButton: some View {
        Button {
            ble.sendStop()
        } label: {
            Text("■ Stop")
                .font(.headline)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 14)
        }
        .buttonStyle(.borderedProminent)
        .tint(.red)
        .disabled(ble.state != .connected)
    }

    private func motionButton(_ title: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.headline)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 14)
        }
        .buttonStyle(.bordered)
        .disabled(ble.state != .connected)
    }
}
