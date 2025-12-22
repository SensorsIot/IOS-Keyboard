import Foundation
import AVFoundation
import Accelerate

/// Service that detects a specific audio frequency using the Goertzel algorithm
/// Used for low-power wake detection in IDLE_DIMMED state
class ToneDetectorService: ObservableObject {
    // MARK: - Published State
    @Published var isDetecting = false
    @Published var toneDetected = false

    // MARK: - Configuration
    private let targetFrequency: Double = 1000.0  // Hz - adjust as needed
    private let sampleRate: Double = 8000.0       // Low sample rate for power efficiency
    private let detectionThreshold: Float = 0.3   // Energy threshold for detection
    private let consecutiveFramesRequired = 3     // Frames needed for confirmed detection

    // MARK: - Private Properties
    private var audioEngine: AVAudioEngine?
    private var consecutiveDetections = 0
    private var onToneDetected: (() -> Void)?

    // Goertzel coefficients (computed once for target frequency)
    private var goertzelCoeff: Float = 0
    private var blockSize: Int = 256

    // MARK: - Init

    init() {
        computeGoertzelCoefficient()
    }

    private func computeGoertzelCoefficient() {
        // Goertzel coefficient: 2 * cos(2 * pi * k / N)
        // where k = targetFrequency * N / sampleRate
        let k = Int(round(targetFrequency * Double(blockSize) / sampleRate))
        let omega = 2.0 * Double.pi * Double(k) / Double(blockSize)
        goertzelCoeff = Float(2.0 * cos(omega))
    }

    // MARK: - Detection Control

    func startDetection(onDetected: @escaping () -> Void) {
        guard !isDetecting else { return }

        self.onToneDetected = onDetected
        consecutiveDetections = 0

        setupAudioEngine()
    }

    func stopDetection() {
        audioEngine?.stop()
        audioEngine?.inputNode.removeTap(onBus: 0)
        audioEngine = nil

        DispatchQueue.main.async {
            self.isDetecting = false
            self.toneDetected = false
        }
    }

    // MARK: - Audio Setup

    private func setupAudioEngine() {
        audioEngine = AVAudioEngine()
        guard let audioEngine = audioEngine else { return }

        let inputNode = audioEngine.inputNode

        // Use a lower sample rate format for power efficiency
        let format = AVAudioFormat(standardFormatWithSampleRate: sampleRate, channels: 1)!

        // Install tap on audio input
        inputNode.installTap(onBus: 0, bufferSize: UInt32(blockSize), format: format) { [weak self] buffer, _ in
            self?.processAudioBuffer(buffer)
        }

        // Configure audio session for low-power operation
        let audioSession = AVAudioSession.sharedInstance()
        do {
            try audioSession.setCategory(.record, mode: .measurement, options: .duckOthers)
            try audioSession.setActive(true)
        } catch {
            print("ToneDetector: Audio session error: \(error)")
            return
        }

        // Start engine
        do {
            try audioEngine.start()
            DispatchQueue.main.async {
                self.isDetecting = true
            }
            print("ToneDetector: Started listening for \(targetFrequency) Hz")
        } catch {
            print("ToneDetector: Failed to start audio engine: \(error)")
        }
    }

    // MARK: - Audio Processing

    private func processAudioBuffer(_ buffer: AVAudioPCMBuffer) {
        guard let channelData = buffer.floatChannelData?[0] else { return }
        let frameCount = Int(buffer.frameLength)

        // Run Goertzel algorithm
        let magnitude = goertzelMagnitude(samples: channelData, count: frameCount)

        // Normalize by frame count
        let normalizedMagnitude = magnitude / Float(frameCount)

        // Check if tone is detected
        if normalizedMagnitude > detectionThreshold {
            consecutiveDetections += 1

            if consecutiveDetections >= consecutiveFramesRequired {
                DispatchQueue.main.async {
                    self.toneDetected = true
                    self.onToneDetected?()
                }
                consecutiveDetections = 0
            }
        } else {
            consecutiveDetections = 0
        }
    }

    /// Goertzel algorithm for single-frequency detection
    /// More efficient than FFT when detecting a single frequency
    private func goertzelMagnitude(samples: UnsafePointer<Float>, count: Int) -> Float {
        var s0: Float = 0
        var s1: Float = 0
        var s2: Float = 0

        for i in 0..<count {
            s0 = samples[i] + goertzelCoeff * s1 - s2
            s2 = s1
            s1 = s0
        }

        // Compute magnitude squared
        let real = s1 - s2 * cos(2.0 * Float.pi * Float(targetFrequency) * Float(count) / Float(sampleRate))
        let imag = s2 * sin(2.0 * Float.pi * Float(targetFrequency) * Float(count) / Float(sampleRate))

        return sqrt(real * real + imag * imag)
    }
}
