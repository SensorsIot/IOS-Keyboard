import SwiftUI
import CoreBluetooth

struct DeviceListView: View {
    @EnvironmentObject var viewModel: MainViewModel

    var body: some View {
        VStack {
            if viewModel.discoveredDevices.isEmpty && !viewModel.isScanning {
                ContentUnavailableView(
                    "No Devices Found",
                    systemImage: "keyboard",
                    description: Text("Tap Scan to find IOS-Keyboard devices")
                )
            } else if viewModel.discoveredDevices.isEmpty && viewModel.isScanning {
                VStack(spacing: 16) {
                    ProgressView()
                        .scaleEffect(1.5)
                    Text("Scanning for devices...")
                        .foregroundColor(.secondary)
                }
            } else {
                List(viewModel.discoveredDevices, id: \.identifier) { device in
                    Button {
                        viewModel.connect(to: device)
                    } label: {
                        HStack {
                            Image(systemName: "keyboard")
                                .foregroundColor(.blue)
                            VStack(alignment: .leading) {
                                Text(device.name ?? "Unknown Device")
                                    .fontWeight(.medium)
                                Text(device.identifier.uuidString.prefix(8) + "...")
                                    .font(.caption)
                                    .foregroundColor(.secondary)
                            }
                            Spacer()
                            Image(systemName: "chevron.right")
                                .foregroundColor(.secondary)
                        }
                    }
                    .foregroundColor(.primary)
                }
            }

            // Scan button
            Button {
                if viewModel.isScanning {
                    viewModel.stopScanning()
                } else {
                    viewModel.startScanning()
                }
            } label: {
                HStack {
                    if viewModel.isScanning {
                        ProgressView()
                            .tint(.white)
                        Text("Stop Scanning")
                    } else {
                        Image(systemName: "antenna.radiowaves.left.and.right")
                        Text("Scan for Devices")
                    }
                }
                .frame(maxWidth: .infinity)
                .padding()
                .background(viewModel.isScanning ? Color.orange : Color.blue)
                .foregroundColor(.white)
                .cornerRadius(12)
            }
            .padding()
        }
    }
}

#Preview {
    DeviceListView()
        .environmentObject(MainViewModel())
}
