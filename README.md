# IOS-Keyboard

ESP32-S3 USB HID keyboard emulator with iOS voice-to-keyboard app.

**Speak into your iPhone → Words appear on any computer.**

## Projects

| Folder | Description |
|--------|-------------|
| [esp32/](esp32/) | ESP32-S3 firmware - USB HID keyboard + BLE receiver |
| [ios/](ios/) | iOS app - Voice recognition + BLE transmitter |
| [Documentation/](Documentation/) | Functional specifications |

## How It Works

```
┌─────────────┐     BLE      ┌─────────────┐     USB      ┌──────────────┐
│   iPhone    │ ──────────▶ │   ESP32-S3  │ ──────────▶ │   Computer   │
│  (Voice)    │   text diff  │  (Firmware) │  keystrokes  │  (Any OS)    │
└─────────────┘              └─────────────┘              └──────────────┘
```

1. Speak into iPhone
2. iOS app converts speech to text
3. App computes minimal diff (backspaces + inserts)
4. Diff sent via BLE to ESP32
5. ESP32 types the text as USB keyboard

## Quick Start

### ESP32 Firmware
```bash
cd esp32
source ~/esp/esp-idf/export.sh
idf.py build
idf.py flash
```

### iOS App
1. Open `ios/IOS-Keyboard-App.xcodeproj` in Xcode
2. Add required permissions to Info.plist
3. Build and run on physical device

## BLE Command Protocol

| Byte 0 | Payload | Action |
|--------|---------|--------|
| `0x02` | UTF-8 text | Type text |
| `0x01` | count (1 byte) | Backspace N times |
| `0x03` | - | Enter key |

## Current Status

- **v2.8.0** - ESP32: WiFi + USB HID + BLE + Keyboard Layouts
- Keyboard layout selectable via web UI (US, Swiss German, German, French, UK, Spanish, Italian)
- iOS app: Voice recognition + BLE - ready for testing

## TODO

- [x] Swiss German keyboard layout support on ESP32
- [ ] iOS app testing and refinement
- [ ] Auto-reconnect on BLE disconnect

## License

MIT
