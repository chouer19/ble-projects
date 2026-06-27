import Combine
import CoreBluetooth
import Foundation

@MainActor
final class SpiderBLEManager: NSObject, ObservableObject {
    enum ConnectionState: Equatable {
        case poweredOff
        case idle
        case scanning
        case connecting
        case connected
        case disconnected
    }

    @Published private(set) var state: ConnectionState = .idle
    @Published private(set) var discoveredPeripherals: [CBPeripheral] = []
    @Published private(set) var connectedPeripheralName: String?
    @Published private(set) var motionDelayMs: UInt16 = 500
    @Published private(set) var servoSpeedDegS: UInt16 = 240
    @Published var lastError: String?

    private var central: CBCentralManager!
    private var cmdCharacteristic: CBCharacteristic?
    private var targetPeripheral: CBPeripheral?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: nil)
    }

    func startScan() {
        guard central.state == .poweredOn else { return }
        discoveredPeripherals.removeAll()
        state = .scanning
        central.scanForPeripherals(withServices: nil, options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false,
        ])
    }

    func stopScan() {
        central.stopScan()
        if state == .scanning {
            state = .idle
        }
    }

    func connect(_ peripheral: CBPeripheral) {
        stopScan()
        targetPeripheral = peripheral
        state = .connecting
        central.connect(peripheral, options: nil)
    }

    func disconnect() {
        guard let peripheral = targetPeripheral else { return }
        central.cancelPeripheralConnection(peripheral)
    }

    func sendMotion(_ motionID: SpiderBLEConstants.MotionID,
                    leg: SpiderBLEConstants.LegID = .none) {
        writeCmd(Data([
            SpiderBLEConstants.Opcode.motion.rawValue,
            motionID.rawValue,
            leg.rawValue,
        ]))
    }

    func sendStop() {
        writeCmd(Data([SpiderBLEConstants.Opcode.stop.rawValue]))
    }

    func sendMotionDelay(_ ms: UInt16) {
        let lo = UInt8(ms & 0xFF)
        let hi = UInt8((ms >> 8) & 0xFF)
        writeCmd(Data([
            SpiderBLEConstants.Opcode.setMotionDelay.rawValue,
            lo,
            hi,
        ]))
        motionDelayMs = ms
    }

    func sendServoSpeed(_ degS: UInt16) {
        let lo = UInt8(degS & 0xFF)
        let hi = UInt8((degS >> 8) & 0xFF)
        writeCmd(Data([
            SpiderBLEConstants.Opcode.setServoSpeed.rawValue,
            lo,
            hi,
        ]))
        servoSpeedDegS = degS
    }

    private func writeCmd(_ data: Data) {
        guard state == .connected,
              let peripheral = targetPeripheral,
              let characteristic = cmdCharacteristic else {
            lastError = "未连接 SpiderBod"
            return
        }

        peripheral.writeValue(data, for: characteristic, type: .withoutResponse)
        lastError = nil
    }

    private func isSpiderBod(_ peripheral: CBPeripheral, advertisementData: [String: Any]) -> Bool {
        if let name = peripheral.name, name.hasPrefix(SpiderBLEConstants.deviceName) {
            return true
        }
        if let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String,
           name.hasPrefix(SpiderBLEConstants.deviceName) {
            return true
        }
        return false
    }
}

extension SpiderBLEManager: CBCentralManagerDelegate {
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        Task { @MainActor in
            switch central.state {
            case .poweredOn:
                if state == .poweredOff {
                    state = .idle
                }
            case .poweredOff:
                state = .poweredOff
            default:
                state = .idle
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDiscover peripheral: CBPeripheral,
                                    advertisementData: [String: Any],
                                    rssi RSSI: NSNumber) {
        Task { @MainActor in
            guard isSpiderBod(peripheral, advertisementData: advertisementData) else { return }
            if !discoveredPeripherals.contains(where: { $0.identifier == peripheral.identifier }) {
                discoveredPeripherals.append(peripheral)
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didConnect peripheral: CBPeripheral) {
        Task { @MainActor in
            targetPeripheral = peripheral
            peripheral.delegate = self
            connectedPeripheralName = peripheral.name ?? SpiderBLEConstants.deviceName
            state = .connected
            peripheral.discoverServices([SpiderBLEConstants.serviceUUID])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didFailToConnect peripheral: CBPeripheral,
                                    error: Error?) {
        Task { @MainActor in
            lastError = error?.localizedDescription ?? "连接失败"
            state = .disconnected
            cmdCharacteristic = nil
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager,
                                    didDisconnectPeripheral peripheral: CBPeripheral,
                                    error: Error?) {
        Task { @MainActor in
            lastError = error?.localizedDescription
            connectedPeripheralName = nil
            cmdCharacteristic = nil
            targetPeripheral = nil
            state = .disconnected
        }
    }
}

extension SpiderBLEManager: CBPeripheralDelegate {
    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didDiscoverServices error: Error?) {
        Task { @MainActor in
            if let error {
                lastError = error.localizedDescription
                return
            }
            guard let service = peripheral.services?.first(where: {
                $0.uuid == SpiderBLEConstants.serviceUUID
            }) else {
                lastError = "未找到 Spider Remote Service"
                return
            }
            peripheral.discoverCharacteristics(
                [SpiderBLEConstants.cmdCharacteristicUUID],
                for: service
            )
        }
    }

    nonisolated func peripheral(_ peripheral: CBPeripheral,
                                didDiscoverCharacteristicsFor service: CBService,
                                error: Error?) {
        Task { @MainActor in
            if let error {
                lastError = error.localizedDescription
                return
            }
            guard let characteristic = service.characteristics?.first(where: {
                $0.uuid == SpiderBLEConstants.cmdCharacteristicUUID
            }) else {
                lastError = "未找到 cmd characteristic"
                return
            }
            cmdCharacteristic = characteristic
            lastError = nil
        }
    }
}
