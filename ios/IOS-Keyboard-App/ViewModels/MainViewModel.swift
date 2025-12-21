import Foundation
import CoreBluetooth
import Combine

@MainActor
class MainViewModel: ObservableObject {
    // MARK: - Services
    private let bluetoothService = BluetoothService()
    private let speechService = SpeechRecognitionService()
    private let diffService = TextDiffService()

    private var cancellables = Set<AnyCancellable>()

    // MARK: - Published State

    // BLE
    @Published var isScanning = false
    @Published var isConnected = false
    @Published var connectedDeviceName: String?
    @Published var discoveredDevices: [CBPeripheral] = []

    // Speech
    @Published var isRecording = false
    @Published var recognizedText = ""
    @Published var transmittedText = ""

    // MARK: - Init

    init() {
        setupBindings()
    }

    private func setupBindings() {
        // Bind Bluetooth service state
        bluetoothService.$isScanning
            .receive(on: DispatchQueue.main)
            .assign(to: &$isScanning)

        bluetoothService.$isConnected
            .receive(on: DispatchQueue.main)
            .assign(to: &$isConnected)

        bluetoothService.$connectedDeviceName
            .receive(on: DispatchQueue.main)
            .assign(to: &$connectedDeviceName)

        bluetoothService.$discoveredDevices
            .receive(on: DispatchQueue.main)
            .assign(to: &$discoveredDevices)

        // Bind Speech service state
        speechService.$isRecording
            .receive(on: DispatchQueue.main)
            .assign(to: &$isRecording)

        // Handle speech recognition results
        speechService.$currentTranscript
            .receive(on: DispatchQueue.main)
            .sink { [weak self] newText in
                self?.handleTranscriptUpdate(newText)
            }
            .store(in: &cancellables)

        // Stop recording if BLE disconnects
        bluetoothService.$isConnected
            .receive(on: DispatchQueue.main)
            .sink { [weak self] connected in
                if !connected && self?.isRecording == true {
                    self?.stopRecording()
                }
            }
            .store(in: &cancellables)
    }

    // MARK: - BLE Actions

    func startScanning() {
        bluetoothService.startScanning()
    }

    func stopScanning() {
        bluetoothService.stopScanning()
    }

    func connect(to peripheral: CBPeripheral) {
        bluetoothService.connect(to: peripheral)
    }

    func disconnect() {
        stopRecording()
        bluetoothService.disconnect()
    }

    // MARK: - Recording Actions

    func startRecording() {
        guard isConnected else { return }

        // Reset state
        recognizedText = ""
        transmittedText = ""
        diffService.reset()

        // Start speech recognition
        speechService.startRecognition()
    }

    func stopRecording() {
        speechService.stopRecognition()

        // Optionally send Enter when stopping
        // bluetoothService.sendEnter()
    }

    // MARK: - Transcript Handling

    private func handleTranscriptUpdate(_ newText: String) {
        // Update recognized text display
        recognizedText = newText

        // Compute diff and send to BLE
        let diff = diffService.computeDiff(newText: newText)

        // Send backspaces if needed
        if diff.backspaces > 0 {
            bluetoothService.sendBackspace(count: UInt8(min(diff.backspaces, 255)))
        }

        // Send new text if any
        if !diff.insert.isEmpty {
            bluetoothService.sendText(diff.insert)
        }

        // Update transmitted text (what the computer should now have)
        transmittedText = newText
    }
}
