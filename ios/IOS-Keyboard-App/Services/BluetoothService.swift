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
    static let ctrlKey: UInt8 = 0x04  // Ctrl + key combo
}

class BluetoothService: NSObject, ObservableObject {
    // MARK: - Published State
    @Published var isScanning = false
    @Published var isConnected = false
    @Published var isAutoConnecting = false  // True when auto-connecting to single device
    @Published var connectedDeviceName: String?
    @Published var discoveredDevices: [CBPeripheral] = []

    // MARK: - Private Properties
    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var lastConnectedPeripheralIdentifier: UUID?
    private var rxCharacteristic: CBCharacteristic?
    private var mtu: Int = 20  // Default BLE MTU, will be updated after connection
    private var autoConnectTimer: Timer?
    private var scanTimeoutTimer: Timer?
    private var shouldAutoReconnect = true

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

        // Scan for all devices to find IOS-Keyboard by name
        centralManager.scanForPeripherals(withServices: nil, options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])

        // Auto-connect after 2 seconds if only one device found
        scanTimeoutTimer?.invalidate()
        scanTimeoutTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: false) { [weak self] _ in
            self?.checkAutoConnect()
        }
    }

    private func checkAutoConnect() {
        guard isScanning, !isConnected else { return }

        if discoveredDevices.count == 1 {
            // Only one device found, auto-connect
            print("BLE: Auto-connecting to single device")
            DispatchQueue.main.async {
                self.isAutoConnecting = true
            }
            connect(to: discoveredDevices[0])
        } else {
            // Multiple or no devices - show device list
            DispatchQueue.main.async {
                self.isAutoConnecting = false
            }
        }
    }

    func stopScanning() {
        scanTimeoutTimer?.invalidate()
        scanTimeoutTimer = nil
        centralManager.stopScan()
        isScanning = false
    }

    // MARK: - Connection

    func connect(to peripheral: CBPeripheral) {
        stopScanning()
        shouldAutoReconnect = true
        connectedPeripheral = peripheral
        lastConnectedPeripheralIdentifier = peripheral.identifier
        peripheral.delegate = self
        centralManager.connect(peripheral, options: nil)
    }

    func disconnect() {
        shouldAutoReconnect = false
        autoConnectTimer?.invalidate()
        autoConnectTimer = nil
        if let peripheral = connectedPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
        cleanup()
    }

    private func cleanup() {
        connectedPeripheral = nil
        rxCharacteristic = nil
        DispatchQueue.main.async {
            self.isConnected = false
            self.isAutoConnecting = false
            self.connectedDeviceName = nil
        }
    }

    private func attemptReconnect() {
        guard shouldAutoReconnect, centralManager.state == .poweredOn else { return }

        print("BLE: Attempting to reconnect...")
        startScanning()
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

    /// Send Ctrl + key combo (e.g., Ctrl+J for newline in Claude)
    func sendCtrlKey(_ key: Character) {
        guard let ascii = key.asciiValue else { return }
        let command = Data([Commands.ctrlKey, ascii])
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
            // Auto-start scanning when Bluetooth is ready
            startScanning()
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

                // If this is the previously connected device, reconnect immediately
                if peripheral.identifier == self.lastConnectedPeripheralIdentifier && self.shouldAutoReconnect {
                    print("BLE: Found previously connected device, reconnecting...")
                    self.connect(to: peripheral)
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

        // Retry after a delay
        if shouldAutoReconnect {
            autoConnectTimer?.invalidate()
            autoConnectTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: false) { [weak self] _ in
                self?.attemptReconnect()
            }
        }
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        print("BLE: Disconnected: \(error?.localizedDescription ?? "No error")")
        cleanup()

        // Auto-reconnect after a delay if not manually disconnected
        if shouldAutoReconnect {
            autoConnectTimer?.invalidate()
            autoConnectTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: false) { [weak self] _ in
                self?.attemptReconnect()
            }
        }
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
