import Foundation
import Speech
import AVFoundation

class SpeechRecognitionService: ObservableObject {
    // MARK: - Published State
    @Published var isRecording = false
    @Published var currentTranscript = ""
    @Published var errorMessage: String?
    @Published var currentLocale: Locale

    // MARK: - Private Properties
    private var speechRecognizer: SFSpeechRecognizer?
    private var recognitionRequest: SFSpeechAudioBufferRecognitionRequest?
    private var recognitionTask: SFSpeechRecognitionTask?
    private let audioEngine = AVAudioEngine()

    // MARK: - Static Properties

    /// Common speech recognition locales
    static let commonLocales: [(id: String, name: String)] = [
        ("de-CH", "Deutsch (Schweiz)"),
        ("de-DE", "Deutsch (Deutschland)"),
        ("en-US", "English (US)"),
        ("en-GB", "English (UK)"),
        ("fr-FR", "Francais (France)"),
        ("fr-CH", "Francais (Suisse)"),
        ("it-IT", "Italiano"),
        ("es-ES", "Espanol"),
        ("pt-BR", "Portugues (Brasil)"),
        ("nl-NL", "Nederlands"),
        ("ja-JP", "Japanese"),
        ("zh-CN", "Chinese (Simplified)"),
    ]

    /// Get all supported locales from the system
    static var supportedLocales: [Locale] {
        Array(SFSpeechRecognizer.supportedLocales())
    }

    // MARK: - Init

    init(locale: Locale = Locale.current) {
        self.currentLocale = locale
        self.speechRecognizer = SFSpeechRecognizer(locale: locale)
    }

    // MARK: - Language Selection

    /// Set the recognition language
    func setLanguage(_ locale: Locale) {
        // Stop any ongoing recognition
        if isRecording {
            stopRecognition()
        }

        currentLocale = locale
        speechRecognizer = SFSpeechRecognizer(locale: locale)
        print("SpeechRecognition: Language set to \(locale.identifier)")
    }

    /// Set language by identifier string (e.g., "de-CH")
    func setLanguage(identifier: String) {
        let locale = Locale(identifier: identifier)
        setLanguage(locale)
    }

    // MARK: - Permissions

    func requestPermissions(completion: @escaping (Bool) -> Void) {
        var speechGranted = false
        var micGranted = false

        let group = DispatchGroup()

        // Request speech recognition permission
        group.enter()
        SFSpeechRecognizer.requestAuthorization { status in
            speechGranted = (status == .authorized)
            group.leave()
        }

        // Request microphone permission
        group.enter()
        AVAudioApplication.requestRecordPermission { granted in
            micGranted = granted
            group.leave()
        }

        group.notify(queue: .main) {
            completion(speechGranted && micGranted)
        }
    }

    // MARK: - Recognition Control

    func startRecognition() {
        // Check if already recording
        guard !isRecording else { return }

        // Check speech recognizer availability
        guard let speechRecognizer = speechRecognizer, speechRecognizer.isAvailable else {
            errorMessage = "Speech recognition not available for \(currentLocale.identifier)"
            return
        }

        // Request permissions first
        requestPermissions { [weak self] granted in
            guard granted else {
                self?.errorMessage = "Permissions not granted"
                return
            }
            self?.startRecognitionInternal()
        }
    }

    private func startRecognitionInternal() {
        // Cancel any existing task
        recognitionTask?.cancel()
        recognitionTask = nil

        // Configure audio session
        let audioSession = AVAudioSession.sharedInstance()
        do {
            try audioSession.setCategory(.record, mode: .measurement, options: .duckOthers)
            try audioSession.setActive(true, options: .notifyOthersOnDeactivation)
        } catch {
            errorMessage = "Audio session error: \(error.localizedDescription)"
            return
        }

        // Create recognition request
        recognitionRequest = SFSpeechAudioBufferRecognitionRequest()
        guard let recognitionRequest = recognitionRequest else {
            errorMessage = "Unable to create recognition request"
            return
        }

        // Configure for continuous recognition
        recognitionRequest.shouldReportPartialResults = true
        recognitionRequest.addsPunctuation = true

        // Get audio input
        let inputNode = audioEngine.inputNode
        let recordingFormat = inputNode.outputFormat(forBus: 0)

        // Install tap on audio input
        inputNode.installTap(onBus: 0, bufferSize: 1024, format: recordingFormat) { [weak self] buffer, _ in
            self?.recognitionRequest?.append(buffer)
        }

        // Start audio engine
        audioEngine.prepare()
        do {
            try audioEngine.start()
        } catch {
            errorMessage = "Audio engine error: \(error.localizedDescription)"
            return
        }

        // Start recognition task
        recognitionTask = speechRecognizer?.recognitionTask(with: recognitionRequest) { [weak self] result, error in
            guard let self = self else { return }

            if let result = result {
                // Update transcript on main thread
                DispatchQueue.main.async {
                    self.currentTranscript = result.bestTranscription.formattedString
                }
            }

            if let error = error {
                DispatchQueue.main.async {
                    self.errorMessage = error.localizedDescription
                    self.stopRecognition()
                }
            }
        }

        DispatchQueue.main.async {
            self.isRecording = true
            self.currentTranscript = ""
        }
    }

    func stopRecognition() {
        // Stop audio engine
        audioEngine.stop()
        audioEngine.inputNode.removeTap(onBus: 0)

        // End recognition request
        recognitionRequest?.endAudio()
        recognitionRequest = nil

        // Cancel task
        recognitionTask?.cancel()
        recognitionTask = nil

        DispatchQueue.main.async {
            self.isRecording = false
        }
    }
}
