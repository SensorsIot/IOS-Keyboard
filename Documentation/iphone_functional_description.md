
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

