import SwiftUI

struct SpeedSettingsView: View {
    @ObservedObject var ble: SpiderBLEManager

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                speedSection(
                    title: "动作节拍（等同串口 speed）",
                    current: "当前：\(ble.motionDelayMs) ms",
                    presets: SpeedPreset.motionDelayPresets
                ) { preset in
                    ble.sendMotionDelay(preset.value)
                }

                speedSection(
                    title: "舵机转速（等同 spider speed）",
                    current: "当前：\(ble.servoSpeedDegS) °/s",
                    presets: SpeedPreset.servoSpeedPresets
                ) { preset in
                    ble.sendServoSpeed(preset.value)
                }

                Text("预设值写入固件；与串口 speed / spider speed 等价。")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            .padding()
        }
        .navigationTitle("速度")
    }

    private func speedSection(title: String,
                              current: String,
                              presets: [SpeedPreset],
                              action: @escaping (SpeedPreset) -> Void) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title)
                .font(.headline)
            Text(current)
                .font(.subheadline)
                .foregroundStyle(.secondary)

            HStack(spacing: 8) {
                ForEach(presets) { preset in
                    Button(preset.label) {
                        action(preset)
                    }
                    .buttonStyle(.bordered)
                    .disabled(ble.state != .connected)
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
    }
}
