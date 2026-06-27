import CoreBluetooth

/// 与固件 include/spider_ble_protocol.h 对齐
enum SpiderBLEConstants {
    static let deviceName = "SpiderBod"

    static let serviceUUID = CBUUID(string: "A0010001-0000-1000-8000-00805F9B34FB")
    static let cmdCharacteristicUUID = CBUUID(string: "A0010002-0000-1000-8000-00805F9B34FB")

    enum Opcode: UInt8 {
        case motion = 0x01
        case stop = 0x02
        case setMotionDelay = 0x10
        case setServoSpeed = 0x11
    }

    enum MotionID: UInt8 {
        case stand = 0x00
        case splay = 0x01
        case lift = 0x02
        case frontDown = 0x03
        case rearDown = 0x04
        case fullDown = 0x05
        case forward = 0x06
        case backward = 0x07
        case turnLeft = 0x08
        case turnRight = 0x09
        case spinLeft = 0x0A
        case spinRight = 0x0B
        case nod = 0x0C
        case shake = 0x0D
        case roll = 0x0E
        case wave = 0x0F
    }

    enum LegID: UInt8 {
        case l1 = 0x00
        case r1 = 0x01
        case l2 = 0x02
        case r2 = 0x03
        case none = 0xFF
    }

    static let motionDelayMinMs: UInt16 = 100
    static let motionDelayMaxMs: UInt16 = 2000
    static let servoSpeedMaxDegS: UInt16 = 600
}
