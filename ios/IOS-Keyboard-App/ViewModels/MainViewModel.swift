import Foundation
import CoreBluetooth
import Combine
import UIKit


// MARK: - UserDefaults Keys

private enum UserDefaultsKeys {
    static let language1 = "speechLanguage1"
    static let language2 = "speechLanguage2"
    static let activeLanguageSlot = "activeLanguageSlot"
}

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
    private var lastDisplayUpdate = Date.distantPast
    private let displayUpdateInterval: TimeInterval = 0.3  // Update display max 3x per second

    // Language Selection
    @Published var language1: String {
        didSet { UserDefaults.standard.set(language1, forKey: UserDefaultsKeys.language1) }
    }
    @Published var language2: String {
        didSet { UserDefaults.standard.set(language2, forKey: UserDefaultsKeys.language2) }
    }
    @Published var activeLanguageSlot: Int {  // 1 or 2
        didSet {
            UserDefaults.standard.set(activeLanguageSlot, forKey: UserDefaultsKeys.activeLanguageSlot)
            applyActiveLanguage()
        }
    }

    /// Available languages for selection
    var availableLanguages: [(id: String, name: String)] {
        SpeechRecognitionService.commonLocales
    }

    /// Current active language identifier
    var currentLanguage: String {
        activeLanguageSlot == 1 ? language1 : language2
    }

    /// Current active language display name
    var currentLanguageName: String {
        let id = currentLanguage
        return availableLanguages.first { $0.id == id }?.name ?? id
    }


    // MARK: - Init

    init() {
        // Load saved languages from UserDefaults
        self.language1 = UserDefaults.standard.string(forKey: UserDefaultsKeys.language1) ?? "de-CH"
        self.language2 = UserDefaults.standard.string(forKey: UserDefaultsKeys.language2) ?? "en-US"
        self.activeLanguageSlot = UserDefaults.standard.integer(forKey: UserDefaultsKeys.activeLanguageSlot)
        if self.activeLanguageSlot == 0 { self.activeLanguageSlot = 1 }  // Default to slot 1

        setupBindings()
        setupIdleTimerDisabled()
        applyActiveLanguage()

        // Restore brightness if it was left dimmed from a previous run
        if UIScreen.main.brightness < 0.2 {
            UIScreen.main.brightness = 0.5
        }
    }

    // MARK: - Language Management

    /// Switch to language slot 1
    func selectLanguage1() {
        activeLanguageSlot = 1
    }

    /// Switch to language slot 2
    func selectLanguage2() {
        activeLanguageSlot = 2
    }

    /// Apply the currently active language to speech service
    private func applyActiveLanguage() {
        let languageId = activeLanguageSlot == 1 ? language1 : language2
        speechService.setLanguage(identifier: languageId)
        print("Language: Switched to slot \(activeLanguageSlot) (\(languageId))")
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

    private func setupIdleTimerDisabled() {
        // Keep the screen on
        UIApplication.shared.isIdleTimerDisabled = true
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
    }

    // MARK: - Transcript Handling

    private func handleTranscriptUpdate(_ newText: String) {
        // Ignore updates when stopping or empty
        guard !isStopping, !newText.isEmpty else { return }

        // Compute diff and send to BLE immediately
        let diff = diffService.computeDiff(newText: newText)

        // Send backspaces if needed
        if diff.backspaces > 0 {
            bluetoothService.sendBackspace(count: UInt8(min(diff.backspaces, 255)))
        }

        // Send new text if any
        if !diff.insert.isEmpty {
            bluetoothService.sendText(diff.insert)
        }

        // Throttle display updates to reduce UI overhead
        let now = Date()
        if now.timeIntervalSince(lastDisplayUpdate) >= displayUpdateInterval {
            recognizedText = newText
            transmittedText = newText
            lastDisplayUpdate = now
        }
    }
}
