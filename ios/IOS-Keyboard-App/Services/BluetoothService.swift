import Foundation
import CoreBluetooth

// Nordic UART Service UUIDs
struct NUSUUIDs {
    static let service = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    static let rxCharacteristic = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E")  // Write to ESP32
    static let txCharacteristic = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E")  // Notify from ESP32
}

// Command bytes
struct Commands {
    static let backspace: UInt8 = 0x01
    static let insert: UInt8 = 0x02
    static let enter: UInt8 = 0x03
}

class BluetoothService: NSObject, ObservableObject {
    // MARK: - Published State
    @Published var isScanning = false
    @Published var isConnected = false
    @Published var connectedDeviceName: String?
    @Published var discoveredDevices: [CBPeripheral] = []

    // MARK: - Private Properties
    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var rxCharacteristic: CBCharacteristic?
    private var mtu: Int = 20  // Default BLE MTU, will be updated after connection

    // MARK: - Init

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    // MARK: - Scanning

    func startScanning() {
        guard centralManager.state == .poweredOn else { return }
        discoveredDevices.removeAll()
        isScanning = true
        // Scan for devices advertising NUS service
        centralManager.scanForPeripherals(withServices: [NUSUUIDs.service], options: nil)

        // Also scan for all devices to find IOS-Keyboard by name
        centralManager.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func stopScanning() {
        centralManager.stopScan()
        isScanning = false
    }

    // MARK: - Connection

    func connect(to peripheral: CBPeripheral) {
        stopScanning()
        connectedPeripheral = peripheral
        peripheral.delegate = self
        centralManager.connect(peripheral, options: nil)
    }

    func disconnect() {
        if let peripheral = connectedPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
        cleanup()
    }

    private func cleanup() {
        connectedPeripheral = nil
        rxCharacteristic = nil
        isConnected = false
        connectedDeviceName = nil
    }

    // MARK: - Command Sending

    /// Send text to be typed on the keyboard
    func sendText(_ text: String) {
        guard let data = text.data(using: .utf8), !data.isEmpty else { return }

        // Chunk data to fit within MTU (minus 1 byte for command)
        let chunkSize = mtu - 1
        var offset = 0

        while offset < data.count {
            let end = min(offset + chunkSize, data.count)
            let chunk = data[offset..<end]

            var command = Data([Commands.insert])
            command.append(chunk)
            sendCommand(command)

            offset = end
        }
    }

    /// Send backspace keystrokes
    func sendBackspace(count: UInt8 = 1) {
        let command = Data([Commands.backspace, count])
        sendCommand(command)
    }

    /// Send Enter keystroke
    func sendEnter() {
        let command = Data([Commands.enter])
        sendCommand(command)
    }

    private func sendCommand(_ data: Data) {
        guard let peripheral = connectedPeripheral,
              let characteristic = rxCharacteristic else {
            print("BLE: Cannot send - not connected")
            return
        }

        peripheral.writeValue(data, for: characteristic, type: .withResponse)
    }
}

// MARK: - CBCentralManagerDelegate

extension BluetoothService: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("BLE: Powered on")
        case .poweredOff:
            print("BLE: Powered off")
            cleanup()
        case .unauthorized:
            print("BLE: Unauthorized")
        case .unsupported:
            print("BLE: Unsupported")
        default:
            break
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        // Filter for IOS-Keyboard devices or devices with NUS service
        let name = peripheral.name ?? ""
        let hasNUS = (advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID])?.contains(NUSUUIDs.service) ?? false

        if name.contains("IOS-Keyboard") || hasNUS {
            if !discoveredDevices.contains(where: { $0.identifier == peripheral.identifier }) {
                DispatchQueue.main.async {
                    self.discoveredDevices.append(peripheral)
                }
            }
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("BLE: Connected to \(peripheral.name ?? "Unknown")")
        DispatchQueue.main.async {
            self.isConnected = true
            self.connectedDeviceName = peripheral.name
        }
        peripheral.discoverServices([NUSUUIDs.service])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        print("BLE: Failed to connect: \(error?.localizedDescription ?? "Unknown error")")
        cleanup()
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("BLE: Disconnected: \(error?.localizedDescription ?? "No error")")
        cleanup()
    }
}

// MARK: - CBPeripheralDelegate

extension BluetoothService: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }

        for service in services where service.uuid == NUSUUIDs.service {
            peripheral.discoverCharacteristics([NUSUUIDs.rxCharacteristic, NUSUUIDs.txCharacteristic], for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }

        for characteristic in characteristics {
            if characteristic.uuid == NUSUUIDs.rxCharacteristic {
                rxCharacteristic = characteristic
                print("BLE: Found RX characteristic")

                // Get the MTU for this connection
                mtu = peripheral.maximumWriteValueLength(for: .withResponse)
                print("BLE: MTU = \(mtu)")
            } else if characteristic.uuid == NUSUUIDs.txCharacteristic {
                peripheral.setNotifyValue(true, for: characteristic)
                print("BLE: Subscribed to TX characteristic")
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        // Handle incoming data from ESP32 (TX characteristic notifications)
        if characteristic.uuid == NUSUUIDs.txCharacteristic, let data = characteristic.value {
            print("BLE: Received \(data.count) bytes from ESP32")
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            print("BLE: Write error: \(error.localizedDescription)")
        }
    }
}
