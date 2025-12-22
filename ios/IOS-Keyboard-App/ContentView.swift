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
        }
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
            // Language selection buttons
            LanguageSelectionBar()

            // Recognized text (what speech recognition heard)
            TextDisplayCard(
                title: "Recognized (\(viewModel.currentLanguageName))",
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

// MARK: - Language Selection Bar

struct LanguageSelectionBar: View {
    @EnvironmentObject var viewModel: MainViewModel
    @State private var showingPicker1 = false
    @State private var showingPicker2 = false

    var body: some View {
        HStack(spacing: 12) {
            // Language 1 Button
            LanguageButton(
                languageId: viewModel.language1,
                languageName: languageName(for: viewModel.language1),
                isActive: viewModel.activeLanguageSlot == 1,
                onTap: { viewModel.selectLanguage1() },
                onLongPress: { showingPicker1 = true }
            )

            // Language 2 Button
            LanguageButton(
                languageId: viewModel.language2,
                languageName: languageName(for: viewModel.language2),
                isActive: viewModel.activeLanguageSlot == 2,
                onTap: { viewModel.selectLanguage2() },
                onLongPress: { showingPicker2 = true }
            )
        }
        .sheet(isPresented: $showingPicker1) {
            LanguagePickerSheet(
                title: "Language 1",
                selectedLanguage: $viewModel.language1,
                availableLanguages: viewModel.availableLanguages
            )
        }
        .sheet(isPresented: $showingPicker2) {
            LanguagePickerSheet(
                title: "Language 2",
                selectedLanguage: $viewModel.language2,
                availableLanguages: viewModel.availableLanguages
            )
        }
    }

    private func languageName(for id: String) -> String {
        viewModel.availableLanguages.first { $0.id == id }?.name ?? id
    }
}

// MARK: - Language Button

struct LanguageButton: View {
    let languageId: String
    let languageName: String
    let isActive: Bool
    let onTap: () -> Void
    let onLongPress: () -> Void

    var body: some View {
        Button(action: onTap) {
            VStack(spacing: 4) {
                Text(flagEmoji(for: languageId))
                    .font(.title)
                Text(shortName(for: languageId))
                    .font(.caption)
                    .fontWeight(isActive ? .bold : .regular)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 12)
            .background(isActive ? Color.blue.opacity(0.2) : Color(.systemGray6))
            .cornerRadius(12)
            .overlay(
                RoundedRectangle(cornerRadius: 12)
                    .stroke(isActive ? Color.blue : Color.clear, lineWidth: 2)
            )
        }
        .buttonStyle(.plain)
        .simultaneousGesture(
            LongPressGesture(minimumDuration: 0.5)
                .onEnded { _ in onLongPress() }
        )
    }

    private func shortName(for id: String) -> String {
        // Return short language name
        switch id {
        case "de-CH": return "DE-CH"
        case "de-DE": return "DE"
        case "en-US": return "EN-US"
        case "en-GB": return "EN-GB"
        case "fr-FR": return "FR"
        case "fr-CH": return "FR-CH"
        case "it-IT": return "IT"
        case "es-ES": return "ES"
        default: return String(id.prefix(5))
        }
    }

    private func flagEmoji(for id: String) -> String {
        switch id {
        case "de-CH", "fr-CH": return "🇨🇭"
        case "de-DE": return "🇩🇪"
        case "en-US": return "🇺🇸"
        case "en-GB": return "🇬🇧"
        case "fr-FR": return "🇫🇷"
        case "it-IT": return "🇮🇹"
        case "es-ES": return "🇪🇸"
        case "pt-BR": return "🇧🇷"
        case "nl-NL": return "🇳🇱"
        case "ja-JP": return "🇯🇵"
        case "zh-CN": return "🇨🇳"
        default: return "🌐"
        }
    }
}

// MARK: - Language Picker Sheet

struct LanguagePickerSheet: View {
    let title: String
    @Binding var selectedLanguage: String
    let availableLanguages: [(id: String, name: String)]
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        NavigationStack {
            List {
                ForEach(availableLanguages, id: \.id) { language in
                    Button(action: {
                        selectedLanguage = language.id
                        dismiss()
                    }) {
                        HStack {
                            Text(flagEmoji(for: language.id))
                            Text(language.name)
                            Spacer()
                            if selectedLanguage == language.id {
                                Image(systemName: "checkmark")
                                    .foregroundColor(.blue)
                            }
                        }
                    }
                    .foregroundColor(.primary)
                }
            }
            .navigationTitle(title)
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .cancellationAction) {
                    Button("Cancel") { dismiss() }
                }
            }
        }
    }

    private func flagEmoji(for id: String) -> String {
        switch id {
        case "de-CH", "fr-CH": return "🇨🇭"
        case "de-DE": return "🇩🇪"
        case "en-US": return "🇺🇸"
        case "en-GB": return "🇬🇧"
        case "fr-FR": return "🇫🇷"
        case "it-IT": return "🇮🇹"
        case "es-ES": return "🇪🇸"
        case "pt-BR": return "🇧🇷"
        case "nl-NL": return "🇳🇱"
        case "ja-JP": return "🇯🇵"
        case "zh-CN": return "🇨🇳"
        default: return "🌐"
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
