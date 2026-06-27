import Foundation

struct SpiderMotionCommand: Identifiable {
    let id = UUID()
    let title: String
    let motionID: SpiderBLEConstants.MotionID
    let legID: SpiderBLEConstants.LegID

    init(_ title: String, _ motionID: SpiderBLEConstants.MotionID,
         leg: SpiderBLEConstants.LegID = .none) {
        self.title = title
        self.motionID = motionID
        self.legID = leg
    }
}

extension SpiderMotionCommand {
    static let remotePanel: [SpiderMotionCommand] = [
        SpiderMotionCommand("Stand", .stand),
        SpiderMotionCommand("Forward", .forward),
        SpiderMotionCommand("Backward", .backward),
        SpiderMotionCommand("Turn L", .turnLeft),
        SpiderMotionCommand("Turn R", .turnRight),
        SpiderMotionCommand("Spin L", .spinLeft),
        SpiderMotionCommand("Spin R", .spinRight),
        SpiderMotionCommand("Splay", .splay),
    ]
}

struct SpeedPreset: Identifiable {
    let id = UUID()
    let label: String
    let value: UInt16
}

extension SpeedPreset {
    static let motionDelayPresets: [SpeedPreset] = [
        SpeedPreset(label: "慢 300", value: 300),
        SpeedPreset(label: "中 500", value: 500),
        SpeedPreset(label: "快 800", value: 800),
    ]

    static let servoSpeedPresets: [SpeedPreset] = [
        SpeedPreset(label: "慢 120", value: 120),
        SpeedPreset(label: "中 240", value: 240),
        SpeedPreset(label: "快 360", value: 360),
    ]
}
