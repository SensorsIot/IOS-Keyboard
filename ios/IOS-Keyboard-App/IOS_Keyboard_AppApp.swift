import SwiftUI
import UIKit

@main
struct IOS_Keyboard_AppApp: App {
    @StateObject private var viewModel = MainViewModel()

    init() {
        // Prevent screen from sleeping while app is active
        UIApplication.shared.isIdleTimerDisabled = true
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(viewModel)
        }
    }
}
