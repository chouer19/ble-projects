import SwiftUI

struct SpeedSettingsView: View {
    @ObservedObject var ble: SpiderBLEManager

    @State private var customDelayMs: Double = 500
    @State private var customServoDegS: Double = 240

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 24) {
                speedSection(
                    title: "动作节拍（等同串口 speed）",
                    current: "当前：\(ble.motionDelayMs) ms",
                    presets: SpeedPreset.motionDelayPresets,
                    customLabel: "自定义 ms",
                    customRange: Double(SpiderBLEConstants.motionDelayMinMs)...Double(SpiderBLEConstants.motionDelayMaxMs),
                    customStep: 50,
                    customValue: $customDelayMs
                ) { preset in
                    ble.sendMotionDelay(preset.value)
                    customDelayMs = Double(preset.value)
                } onApplyCustom: {
                    ble.sendMotionDelay(UInt16(customDelayMs))
                }

                speedSection(
                    title: "舵机转速（等同 spider speed）",
                    current: "当前：\(ble.servoSpeedDegS) °/s（0 = 瞬时）",
                    presets: SpeedPreset.servoSpeedPresets,
                    customLabel: "自定义 °/s",
                    customRange: 0...Double(SpiderBLEConstants.servoSpeedMaxDegS),
                    customStep: 30,
                    customValue: $customServoDegS
                ) { preset in
                    ble.sendServoSpeed(preset.value)
                    customServoDegS = Double(preset.value)
                } onApplyCustom: {
                    ble.sendServoSpeed(UInt16(customServoDegS))
                }

                Text("BLE 可设全部 Motion 与两种 speed。舵机标定、angle 等调试命令仍请用串口。")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
            .padding()
        }
        .navigationTitle("速度")
        .onAppear {
            customDelayMs = Double(ble.motionDelayMs)
            customServoDegS = Double(ble.servoSpeedDegS)
        }
    }

    private func speedSection(title: String,
                              current: String,
                              presets: [SpeedPreset],
                              customLabel: String,
                              customRange: ClosedRange<Double>,
                              customStep: Double,
                              customValue: Binding<Double>,
                              onPreset: @escaping (SpeedPreset) -> Void,
                              onApplyCustom: @escaping () -> Void) -> some View {
        VStack(alignment: .leading, spacing: 12) {
            Text(title)
                .font(.headline)
            Text(current)
                .font(.subheadline)
                .foregroundStyle(.secondary)

            HStack(spacing: 8) {
                ForEach(presets) { preset in
                    Button(preset.label) {
                        onPreset(preset)
                    }
                    .buttonStyle(.bordered)
                    .disabled(ble.state != .connected)
                }
            }

            HStack {
                Text(customLabel)
                    .font(.subheadline)
                Spacer()
                Text("\(Int(customValue.wrappedValue))")
                    .font(.subheadline.monospacedDigit())
                    .foregroundStyle(.secondary)
            }

            Slider(value: customValue, in: customRange, step: customStep)
                .disabled(ble.state != .connected)

            Button("应用 \(Int(customValue.wrappedValue))") {
                onApplyCustom()
            }
            .buttonStyle(.borderedProminent)
            .disabled(ble.state != .connected)
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
    }
}
