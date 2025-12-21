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
    @Published var isAutoConnecting = false
    @Published var connectedDeviceName: String?
    @Published var discoveredDevices: [CBPeripheral] = []

    // Speech
    @Published var isRecording = false
    @Published var recognizedText = ""
    @Published var transmittedText = ""
    private var isStopping = false  // Flag to ignore updates during stop

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

        bluetoothService.$isAutoConnecting
            .receive(on: DispatchQueue.main)
            .assign(to: &$isAutoConnecting)

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
        // Set flag to ignore any pending transcript updates
        isStopping = true

        speechService.stopRecognition()

        // Clear display and reset for next recording (doesn't delete text on target)
        recognizedText = ""
        transmittedText = ""
        diffService.reset()

        // Reset flag after a brief delay to ensure all pending updates are ignored
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
            self.isStopping = false
        }

        // Optionally send Enter when stopping
        // bluetoothService.sendEnter()
    }

    // MARK: - Transcript Handling

    /// Magic word to trigger Ctrl+J (newline in Claude prompt)
    private let magicWord = "Abrahadabra"

    private func handleTranscriptUpdate(_ newText: String) {
        // Ignore updates when stopping or empty
        guard !isStopping, !newText.isEmpty else { return }

        // Check for magic word and replace with Ctrl+J
        let processedText = processMagicWords(newText)

        // Update recognized text display
        recognizedText = newText

        // Compute diff and send to BLE
        let diff = diffService.computeDiff(newText: processedText)

        // Send backspaces if needed
        if diff.backspaces > 0 {
            bluetoothService.sendBackspace(count: UInt8(min(diff.backspaces, 255)))
        }

        // Send new text if any
        if !diff.insert.isEmpty {
            sendTextWithMagicWords(diff.insert)
        }

        // Update transmitted text (what the computer should now have)
        transmittedText = processedText
    }

    /// Replace magic word with a placeholder for diff calculation
    private func processMagicWords(_ text: String) -> String {
        // Replace "Abrahadabra" with newline character for diff tracking
        return text.replacingOccurrences(of: magicWord, with: "\n", options: .caseInsensitive)
    }

    /// Send text, replacing magic word occurrences with Ctrl+J
    private func sendTextWithMagicWords(_ text: String) {
        // Split by magic word (case insensitive)
        let parts = text.components(separatedBy: "\n")

        for (index, part) in parts.enumerated() {
            // Send the text part
            if !part.isEmpty {
                bluetoothService.sendText(part)
            }

            // Send Ctrl+J between parts (not after the last one)
            if index < parts.count - 1 {
                bluetoothService.sendCtrlKey("J")
            }
        }
    }
}
