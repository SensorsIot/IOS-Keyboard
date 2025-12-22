import Foundation
import CoreBluetooth
import Combine
import UIKit

// MARK: - Power State

enum PowerState {
    case activeListening
    case idleDimmed
    case wakeTransition
}

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
    private let toneDetector = ToneDetectorService()

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

    // Power Management
    @Published var powerState: PowerState = .activeListening
    private var lastActivityTime = Date()
    private var silenceTimer: Timer?
    private var savedBrightness: CGFloat = 1.0

    // Configuration
    private let endOfSpeechTimeout: TimeInterval = 5.0  // seconds of silence before dimming
    private let dimmedBrightness: CGFloat = 0.05

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
        transitionTo(.activeListening)  // Reset power state
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

        // Start power management
        transitionTo(.activeListening)
        startSilenceMonitor()
    }

    func stopRecording() {
        // Set flag to ignore any pending transcript updates
        isStopping = true

        speechService.stopRecognition()
        stopSilenceMonitor()
        toneDetector.stopDetection()

        // Clear display and reset for next recording (doesn't delete text on target)
        recognizedText = ""
        transmittedText = ""
        diffService.reset()

        // Reset power state
        transitionTo(.activeListening)

        // Reset flag after a brief delay to ensure all pending updates are ignored
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
            self.isStopping = false
        }
    }

    // MARK: - Power State Management

    private func transitionTo(_ newState: PowerState) {
        guard newState != powerState else { return }

        let oldState = powerState
        print("PowerState: \(oldState) -> \(newState)")

        switch newState {
        case .activeListening:
            enterActiveListening(from: oldState)
        case .idleDimmed:
            enterIdleDimmed()
        case .wakeTransition:
            enterWakeTransition()
        }

        powerState = newState
    }

    private func enterActiveListening(from previousState: PowerState) {
        // Restore brightness if coming from dimmed state
        if previousState == .idleDimmed || previousState == .wakeTransition {
            restoreBrightness()
        }

        // Start silence monitor
        startSilenceMonitor()
        lastActivityTime = Date()
    }

    private func enterIdleDimmed() {
        // Stop speech recognition
        speechService.stopRecognition()

        // Stop silence monitor (no longer needed)
        stopSilenceMonitor()

        // Dim the display
        saveBrightness()
        setDimmedBrightness()

        // Start tone detector for wake-up
        toneDetector.startDetection { [weak self] in
            self?.onToneDetected()
        }

        print("PowerState: Entered IDLE_DIMMED - listening for wake tone")
    }

    private func enterWakeTransition() {
        // Stop tone detector
        toneDetector.stopDetection()

        // Restore brightness
        restoreBrightness()

        // Restart speech recognition
        speechService.startRecognition()

        // Transition to active listening once speech is ready
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            self?.transitionTo(.activeListening)
        }
    }

    // MARK: - Silence Monitor

    private func startSilenceMonitor() {
        stopSilenceMonitor()  // Clear any existing timer

        silenceTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            Task { @MainActor in
                self?.checkSilenceTimeout()
            }
        }
    }

    private func stopSilenceMonitor() {
        silenceTimer?.invalidate()
        silenceTimer = nil
    }

    private func checkSilenceTimeout() {
        guard powerState == .activeListening, isRecording else { return }

        let silenceDuration = Date().timeIntervalSince(lastActivityTime)
        if silenceDuration > endOfSpeechTimeout {
            print("PowerState: Silence timeout (\(silenceDuration)s) - transitioning to IDLE_DIMMED")
            transitionTo(.idleDimmed)
        }
    }

    // MARK: - Tone Detection

    private func onToneDetected() {
        guard powerState == .idleDimmed else { return }
        print("PowerState: Wake tone detected!")
        transitionTo(.wakeTransition)
    }

    // MARK: - Brightness Management

    private func saveBrightness() {
        savedBrightness = UIScreen.main.brightness
    }

    private func setDimmedBrightness() {
        UIScreen.main.brightness = dimmedBrightness
    }

    private func restoreBrightness() {
        UIScreen.main.brightness = savedBrightness
    }

    // MARK: - Transcript Handling

    /// Magic word to trigger Ctrl+J (newline in Claude prompt)
    private let magicWord = "Abrahadabra"

    private func handleTranscriptUpdate(_ newText: String) {
        // Ignore updates when stopping or empty
        guard !isStopping, !newText.isEmpty else { return }

        // Update last activity time for silence detection
        lastActivityTime = Date()

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
