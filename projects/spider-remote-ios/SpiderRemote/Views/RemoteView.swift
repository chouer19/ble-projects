import SwiftUI

struct RemoteView: View {
    @ObservedObject var ble: SpiderBLEManager

    var body: some View {
        ScrollView {
            VStack(spacing: 20) {
                connectionBadge

                stopBar

                categorySection(.posture, columns: 2)
                categorySection(.gait, columns: 2)
                categorySection(.lieDown, columns: 1)
                categorySection(.body, columns: 3)
                legSection(
                    title: SpiderMotionCategory.lift.rawValue,
                    hint: "静态；先进三脚架再抬腿",
                    motions: SpiderMotionDef.liftByLeg
                )
                legSection(
                    title: SpiderMotionCategory.wave.rawValue,
                    hint: "循环；Stop 恢复立正",
                    motions: SpiderMotionDef.waveByLeg
                )

                Text("点按发送一条命令。标「循环」的动作需 Stop 或切换其他动作停止。")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
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

    private var stopBar: some View {
        Button {
            ble.sendStop()
        } label: {
            Text("■ Stop（恢复立正）")
                .font(.headline)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 14)
        }
        .buttonStyle(.borderedProminent)
        .tint(.red)
        .disabled(ble.state != .connected)
    }

    private func categorySection(_ category: SpiderMotionCategory, columns: Int) -> some View {
        let motions = SpiderMotionDef.inCategory(category)
        return motionPanel(title: category.rawValue) {
            LazyVGrid(columns: gridColumns(columns), spacing: 10) {
                ForEach(motions) { motion in
                    motionButton(motion)
                }
            }
        }
    }

    private func legSection(title: String, hint: String, motions: [SpiderMotionDef]) -> some View {
        motionPanel(title: title, subtitle: hint) {
            LazyVGrid(columns: gridColumns(4), spacing: 10) {
                ForEach(motions) { motion in
                    motionButton(motion)
                }
            }
        }
    }

    private func motionPanel<Content: View>(title: String,
                                            subtitle: String? = nil,
                                            @ViewBuilder content: () -> Content) -> some View {
        VStack(alignment: .leading, spacing: 10) {
            Text(title)
                .font(.headline)
            if let subtitle {
                Text(subtitle)
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
            content()
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 12))
    }

    private func motionButton(_ motion: SpiderMotionDef) -> some View {
        Button {
            ble.sendMotion(motion.motionID, leg: motion.legID)
        } label: {
            VStack(spacing: 4) {
                Text(motion.title)
                    .font(.subheadline)
                    .fontWeight(.semibold)
                    .multilineTextAlignment(.center)
                if motion.isCyclic {
                    Text("循环")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 12)
        }
        .buttonStyle(.bordered)
        .disabled(ble.state != .connected)
    }

    private func gridColumns(_ count: Int) -> [GridItem] {
        Array(repeating: GridItem(.flexible(), spacing: 10), count: count)
    }
}
