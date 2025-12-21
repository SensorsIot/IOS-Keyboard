# IOS-Keyboard iOS App

Voice-to-keyboard iOS app for the ESP32-S3 USB HID Keyboard.

Speak into your iPhone, and your words appear on the connected computer.

## Features

- Continuous speech recognition using iOS Speech framework
- Real-time text streaming via BLE to ESP32
- Smart diff algorithm - only sends changes, not full text
- Dual display: "Recognized" vs "Transmitted" text comparison
- iPhone and iPad support

## Requirements

- iOS 15.0+
- iPhone/iPad with Bluetooth 4.0+
- Xcode 15+

## Permissions Required

Add to Info.plist:
```xml
<key>NSMicrophoneUsageDescription</key>
<string>Microphone access is needed for voice recognition</string>
<key>NSSpeechRecognitionUsageDescription</key>
<string>Speech recognition is used to convert your voice to text</string>
<key>NSBluetoothAlwaysUsageDescription</key>
<string>Bluetooth is used to connect to the keyboard device</string>
```

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                   MainViewModel                      │
│  (Orchestrates speech, diff, and BLE services)      │
└─────────────────────────────────────────────────────┘
         │                │                │
         ▼                ▼                ▼
┌─────────────┐  ┌─────────────┐  ┌─────────────────┐
│   Speech    │  │  TextDiff   │  │   Bluetooth     │
│ Recognition │  │   Service   │  │    Service      │
│   Service   │  │             │  │                 │
└─────────────┘  └─────────────┘  └─────────────────┘
```

## BLE Protocol

Communicates with ESP32 using Nordic UART Service (NUS):

| Command | Format | Description |
|---------|--------|-------------|
| Insert | `0x02` + text | Type text characters |
| Backspace | `0x01` + count | Send N backspaces |
| Enter | `0x03` | Send Enter key |

### Service UUIDs

- **Service:** `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- **RX (Write):** `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- **TX (Notify):** `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`

## Diff Algorithm

When speech recognition updates the text, only the changes are sent:

```
Old: "Hello wor"
New: "Hello world"
→ Send: insert "ld" (no backspaces)

Old: "Hello word"
New: "Hello world"
→ Send: 1 backspace + insert "ld"
```

## Project Structure

```
ios/
├── IOS-Keyboard-App/
│   ├── IOS_Keyboard_AppApp.swift    # App entry point
│   ├── ContentView.swift             # Main UI + voice interface
│   ├── Views/
│   │   └── DeviceListView.swift      # BLE device scanner
│   ├── ViewModels/
│   │   └── MainViewModel.swift       # State management
│   └── Services/
│       ├── BluetoothService.swift    # CoreBluetooth wrapper
│       ├── SpeechRecognitionService.swift  # Voice recognition
│       └── TextDiffService.swift     # Diff computation
├── IOS-Keyboard-App.xcodeproj/
└── README.md
```

## Building

1. Open `IOS-Keyboard-App.xcodeproj` in Xcode
2. Select your development team in Signing & Capabilities
3. Add required permissions to Info.plist
4. Build and run on a physical iOS device (BLE + mic require real hardware)

## Usage

1. Power on the ESP32-S3 keyboard device
2. Connect ESP32 to a computer via USB
3. Open the iOS app
4. Tap "Scan" and select "IOS-Keyboard"
5. Tap the microphone button to start recording
6. Speak - your words will be typed on the computer
7. Tap stop when done
