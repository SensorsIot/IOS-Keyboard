
# Functional Description: iPhone Voice-to-BLE Streaming App

## Overview
This document defines the functional behavior of an iOS application that performs continuous speech-to-text conversion and streams incremental text updates to an ESP32 device over BLE. No platform-specific UI or boilerplate is described; only functional responsibilities required for later code generation.

## Functional Modules

### 1. Speech Recognition Module
**Responsibilities**
- Initialize and request permissions for speech recognition and microphone access.
- Start continuous speech recognition using `SFSpeechRecognizer` and `AVAudioEngine`.
- Receive continuous partial transcription updates.
- Report final transcription when speech recognizer finalizes a segment.

**Functions**
1. `startRecognition()`
   - Requests permissions.
   - Initializes recognition engine and audio engine.
   - Begins streaming microphone audio to the recognizer.

2. `stopRecognition()`
   - Stops audio engine.
   - Ends recognition request and cancels recognition task.
   - Resets internal state.

3. `onPartialResult(transcript: String, isFinal: Bool)`
   - Triggered by speech recognizer.
   - Provides incremental transcript text.

---

### 2. Text Diff Module
**Responsibilities**
- Maintain internal variable representing previously-sent text.
- Compute differences between new partial transcript and last-sent transcript.
- Produce a minimal set of edit operations (backspaces + inserted text) for BLE transfer.

**Functions**
1. `computeDiff(newText: String) -> (backspaces: Int, insert: String)`
   - Determines common prefix length.
   - Calculates number of trailing characters to delete.
   - Extracts new characters to append.

2. `updateLastSent(newText: String)`
   - Store the latest transmitted text.

---

### 3. BLE Communication Module (Central Role)
**Responsibilities**
- Scan and connect to a BLE peripheral exposing a UART-like service.
- Retrieve the write characteristic (RX).
- Send encoded diff commands via BLE WriteWithoutResponse.

**Functions**
1. `startBLEScan()`
   - Search for peripherals advertising the target UART service UUID.

2. `connectToPeripheral(identifier)`
   - Initiate connection.
   - Discover services and characteristics.

3. `setWriteCharacteristic(characteristic)`
   - Store reference to RX characteristic used for sending text.

4. `sendBackspace(count: Int)`
   - Encode command type = 0x01.
   - Send one or more packets depending on count.

5. `sendInsert(text: String)`
   - Encode command type = 0x02.
   - Chunk text to MTU size and send each block.

6. `sendEnter()`
   - Encode command type = 0x03.

---

### 4. Application Control Logic
**Responsibilities**
- Orchestrate speech recognition, diff calculation, and BLE transmission.
- Respond to start/stop actions initiated by user/UI.
- Reset state when recognition stops.

**Functions**
1. `start()`
   - Initiates BLE scanning and connection.
   - Starts speech recognition once BLE link is active.

2. `stop()`
   - Stops speech recognition.
   - Optionally sends an Enter command.
   - Clears local state (e.g., last-sent text).

---

### 5. Tone Detector Module
**Responsibilities**
- Lightweight audio processing to detect a specific wake-up frequency.
- Runs during IDLE_DIMMED state as a low-power alternative to speech recognition.
- Uses Goertzel algorithm or narrow-band FFT for efficient single-frequency detection.

**Functions**
1. `startDetection(targetFrequency: Double)`
   - Configures audio input for low-rate sampling.
   - Begins monitoring for target frequency.

2. `stopDetection()`
   - Stops audio processing.
   - Releases audio resources.

3. `onToneDetected()`
   - Callback triggered when tone energy exceeds threshold for N consecutive frames.

---

## Power Management

### State Definitions

#### STATE: ACTIVE_LISTENING
- Continuous speech recognition active (`SFSpeechRecognizer` + `AVAudioEngine`).
- BLE connection interval = fast (15–30 ms).
- Display on (normal brightness or dimmed as defined by app UI).
- Idle timer disabled (`isIdleTimerDisabled = true`).
- UI allowed to update transcription (if enabled).

#### STATE: IDLE_DIMMED
- Speech recognition fully stopped.
- Tone detector active (lightweight audio processing for a single frequency).
- BLE connection interval = slow (100–400 ms).
- Display replaced with full-screen black view.
- Brightness set to very low (e.g., 0.05).
- Idle timer disabled (`isIdleTimerDisabled = true`).
- No UI updates except state change.

#### STATE: WAKE_TRANSITION
- Transition state after tone detection.
- Tone detector stopped.
- Brightness and UI restored to normal view.
- BLE connection interval returned to fast.
- Speech recognition starting.

### State Transitions

#### ACTIVE_LISTENING → IDLE_DIMMED
**Trigger:**
- No changes in recognizer partial results for `endOfSpeechTimeout` seconds (3–5 s).

**Actions:**
1. Stop speech recognition.
2. Send/flush final text (if any) to BLE.
3. Switch BLE to slow interval.
4. Replace UI with black view.
5. Save brightness; set brightness to low.
6. Start tone detector.
7. Keep idle timer disabled.

#### IDLE_DIMMED → WAKE_TRANSITION
**Trigger:**
- Tone detector identifies target frequency energy above threshold for N consecutive frames.

**Actions:**
1. Stop tone detector.
2. Restore brightness.
3. Restore UI to listening screen.
4. Switch BLE to fast interval.

#### WAKE_TRANSITION → ACTIVE_LISTENING
**Trigger:**
- Speech recognizer has restarted successfully.

**Actions:**
1. Start speech recognition (`SFSpeechRecognizer` + `AVAudioEngine`).
2. Begin tracking `lastActivityTime`.
3. Resume normal diff → BLE sending behavior.

### Timers and Monitors

#### Silence Monitor (Active Listening)
Every 1 second:
```
if now - lastActivityTime > endOfSpeechTimeout:
    transition to IDLE_DIMMED
```

#### Tone Detector Monitor (Idle Dimmed)
- Runs continuously on low-rate audio input.
- Implements Goertzel or narrow-band FFT for target tone.
- N consecutive detections → transition to WAKE_TRANSITION.

### BLE Behavior

#### Fast Mode (ACTIVE_LISTENING)
- Connection interval: 15–30 ms.
- Low latency for rapid diff transmission.

#### Slow Mode (IDLE_DIMMED)
- Connection interval: 100–400 ms.
- Minimal radio activity.
- No BLE writes unless state changes.

### Display Management

#### In ACTIVE_LISTENING
- Brightness = normal or user setting.
- UI visible, input feedback allowed.

#### In IDLE_DIMMED
- UI = full black view.
- Brightness near zero.
- Remains unlocked; device stays awake.

#### In WAKE_TRANSITION
- Brightness restored.
- UI restored.

---

