import Foundation

/// 与固件 kame_motion_id_t / 串口顶层命令一一对应
enum SpiderMotionCategory: String, CaseIterable, Identifiable {
    case posture = "姿态"
    case gait = "行走"
    case lieDown = "趴下"
    case body = "机身摆动"
    case lift = "抬腿"
    case wave = "招手"

    var id: String { rawValue }
}

struct SpiderMotionDef: Identifiable {
    let id: String
    let title: String
    let motionID: SpiderBLEConstants.MotionID
    let legID: SpiderBLEConstants.LegID
    let isCyclic: Bool
    let category: SpiderMotionCategory

    init(_ title: String,
         _ motionID: SpiderBLEConstants.MotionID,
         category: SpiderMotionCategory,
         leg: SpiderBLEConstants.LegID = .none,
         cyclic: Bool = false) {
        self.id = "\(motionID.rawValue)-\(leg.rawValue)"
        self.title = title
        self.motionID = motionID
        self.legID = leg
        self.isCyclic = cyclic
        self.category = category
    }
}

extension SpiderMotionDef {
    static let legs: [(SpiderBLEConstants.LegID, String)] = [
        (.l1, "L1"),
        (.r1, "R1"),
        (.l2, "L2"),
        (.r2, "R2"),
    ]

    static let all: [SpiderMotionDef] = [
        // 姿态
        SpiderMotionDef("Stand", .stand, category: .posture),
        SpiderMotionDef("Splay", .splay, category: .posture),
        // 行走（循环）
        SpiderMotionDef("Forward", .forward, category: .gait, cyclic: true),
        SpiderMotionDef("Backward", .backward, category: .gait, cyclic: true),
        SpiderMotionDef("Turn L", .turnLeft, category: .gait, cyclic: true),
        SpiderMotionDef("Turn R", .turnRight, category: .gait, cyclic: true),
        SpiderMotionDef("Spin L", .spinLeft, category: .gait, cyclic: true),
        SpiderMotionDef("Spin R", .spinRight, category: .gait, cyclic: true),
        // 趴下
        SpiderMotionDef("Front Down", .frontDown, category: .lieDown),
        SpiderMotionDef("Rear Down", .rearDown, category: .lieDown),
        SpiderMotionDef("Full Down", .fullDown, category: .lieDown),
        // 机身摆动（循环）
        SpiderMotionDef("Nod", .nod, category: .body, cyclic: true),
        SpiderMotionDef("Shake", .shake, category: .body, cyclic: true),
        SpiderMotionDef("Roll", .roll, category: .body, cyclic: true),
    ]

    static func lift(_ leg: SpiderBLEConstants.LegID, name: String) -> SpiderMotionDef {
        SpiderMotionDef("Lift \(name)", .lift, category: .lift, leg: leg)
    }

    static func wave(_ leg: SpiderBLEConstants.LegID, name: String) -> SpiderMotionDef {
        SpiderMotionDef("Wave \(name)", .wave, category: .wave, leg: leg, cyclic: true)
    }

    static var liftByLeg: [SpiderMotionDef] {
        legs.map { lift($0.0, name: $0.1) }
    }

    static var waveByLeg: [SpiderMotionDef] {
        legs.map { wave($0.0, name: $0.1) }
    }

    static func inCategory(_ category: SpiderMotionCategory) -> [SpiderMotionDef] {
        all.filter { $0.category == category }
    }
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
