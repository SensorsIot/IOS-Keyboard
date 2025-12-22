import SwiftUI

struct ContentView: View {
    @EnvironmentObject var viewModel: MainViewModel

    var body: some View {
        ZStack {
            // Main content
            NavigationStack {
                VStack(spacing: 0) {
                    // Connection status bar
                    ConnectionStatusBar(
                        isConnected: viewModel.isConnected,
                        deviceName: viewModel.connectedDeviceName
                    )

                    if viewModel.isConnected {
                        // Main voice interface
                        VoiceInterfaceView()
                    } else if viewModel.isAutoConnecting || (viewModel.isScanning && viewModel.discoveredDevices.count <= 1) {
                        // Auto-connecting or scanning with 0-1 devices - show simple connecting view
                        ConnectingView()
                    } else {
                        // Multiple devices found or no devices after scan - show device list
                        DeviceListView()
                    }
                }
                .navigationTitle("IOS-Keyboard")
                .navigationBarTitleDisplayMode(.inline)
            }

            // Black screen overlay for IDLE_DIMMED state
            if viewModel.powerState == .idleDimmed {
                IdleDimmedOverlay()
            }
        }
    }
}

// MARK: - Idle Dimmed Overlay

struct IdleDimmedOverlay: View {
    var body: some View {
        Color.black
            .ignoresSafeArea()
            .overlay(
                VStack(spacing: 20) {
                    Image(systemName: "moon.zzz.fill")
                        .font(.system(size: 48))
                        .foregroundColor(.gray.opacity(0.3))
                    Text("Listening for wake tone...")
                        .font(.caption)
                        .foregroundColor(.gray.opacity(0.3))
                }
            )
    }
}

// MARK: - Connecting View

struct ConnectingView: View {
    var body: some View {
        VStack(spacing: 20) {
            Spacer()
            ProgressView()
                .scaleEffect(2)
            Text("Connecting...")
                .font(.title2)
                .foregroundColor(.secondary)
            Spacer()
        }
    }
}

// MARK: - Connection Status Bar

struct ConnectionStatusBar: View {
    let isConnected: Bool
    let deviceName: String?

    var body: some View {
        HStack {
            Image(systemName: isConnected ? "checkmark.circle.fill" : "circle.dashed")
                .foregroundColor(isConnected ? .green : .secondary)
            Text(isConnected ? (deviceName ?? "Connected") : "Not Connected")
                .font(.subheadline)
            Spacer()
        }
        .padding(.horizontal)
        .padding(.vertical, 8)
        .background(Color(.systemGray6))
    }
}

// MARK: - Voice Interface

struct VoiceInterfaceView: View {
    @EnvironmentObject var viewModel: MainViewModel

    var body: some View {
        VStack(spacing: 16) {
            // Power state indicator
            if viewModel.isRecording {
                PowerStateIndicator(state: viewModel.powerState)
            }

            // Recognized text (what speech recognition heard)
            TextDisplayCard(
                title: "Recognized",
                text: viewModel.recognizedText,
                backgroundColor: .blue.opacity(0.1)
            )

            // Transmitted text (what was actually sent to keyboard)
            TextDisplayCard(
                title: "Transmitted",
                text: viewModel.transmittedText,
                backgroundColor: .green.opacity(0.1)
            )

            Spacer()

            // Recording button
            RecordButton(
                isRecording: viewModel.isRecording,
                onTap: {
                    if viewModel.isRecording {
                        viewModel.stopRecording()
                    } else {
                        viewModel.startRecording()
                    }
                }
            )

            // Disconnect button
            Button("Disconnect") {
                viewModel.disconnect()
            }
            .foregroundColor(.red)
            .padding(.bottom)
        }
        .padding()
    }
}

// MARK: - Power State Indicator

struct PowerStateIndicator: View {
    let state: PowerState

    var body: some View {
        HStack {
            Circle()
                .fill(stateColor)
                .frame(width: 8, height: 8)
            Text(stateText)
                .font(.caption)
                .foregroundColor(.secondary)
            Spacer()
        }
        .padding(.horizontal)
    }

    private var stateColor: Color {
        switch state {
        case .activeListening:
            return .green
        case .idleDimmed:
            return .orange
        case .wakeTransition:
            return .yellow
        }
    }

    private var stateText: String {
        switch state {
        case .activeListening:
            return "Listening..."
        case .idleDimmed:
            return "Idle (dimmed)"
        case .wakeTransition:
            return "Waking up..."
        }
    }
}

// MARK: - Text Display Card

struct TextDisplayCard: View {
    let title: String
    let text: String
    let backgroundColor: Color

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text(title)
                .font(.caption)
                .fontWeight(.semibold)
                .foregroundColor(.secondary)

            ScrollView {
                Text(text.isEmpty ? "..." : text)
                    .font(.body)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .frame(maxHeight: 150)
        }
        .padding()
        .background(backgroundColor)
        .cornerRadius(12)
    }
}

// MARK: - Record Button

struct RecordButton: View {
    let isRecording: Bool
    let onTap: () -> Void

    var body: some View {
        Button(action: onTap) {
            ZStack {
                Circle()
                    .fill(isRecording ? Color.red : Color.blue)
                    .frame(width: 80, height: 80)

                if isRecording {
                    RoundedRectangle(cornerRadius: 4)
                        .fill(Color.white)
                        .frame(width: 28, height: 28)
                } else {
                    Image(systemName: "mic.fill")
                        .font(.system(size: 32))
                        .foregroundColor(.white)
                }
            }
        }
        .padding()
    }
}

#Preview {
    ContentView()
        .environmentObject(MainViewModel())
}
