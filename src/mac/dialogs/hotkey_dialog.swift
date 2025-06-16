import SwiftUI
import AppKit
import Foundation

// MARK: - Hotkey Dialog
struct HotkeyDialogView: View {
    @ObservedObject var state: DialogState
    let title: String
    let message: String
    @State private var isCapturing = false
    @State private var capturedKeys = "Press keys..."
    @State private var capturedCombination: KeyCombination = KeyCombination(keys: (KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0)), count: 0)
    @State private var hasValidCombination = false
    @State private var lastFlags: NSEvent.ModifierFlags = []
    @State private var currentKeys: Set<UInt16> = []
    @State private var currentModifiers: NSEvent.ModifierFlags = []
    @State private var allPressedKeys: Set<UInt16> = []
    @State private var maxModifiers: NSEvent.ModifierFlags = []
    @State private var pushToToggle = false

    var body: some View {
        VStack(spacing: 0) {
            // Header with icon and title
            HStack(spacing: 12) {
                if let appIcon = loadAppIcon() {
                    Image(nsImage: appIcon)
                        .resizable()
                        .frame(width: 32, height: 32)
                }
                
                Text(title)
                    .font(.title2)
                    .fontWeight(.medium)
                
                Spacer()
            }
            .padding(.horizontal, 20)
            .padding(.top, 20)
            .padding(.bottom, 24)
            
            // Content area
            VStack(spacing: 20) {
                Text(message)
                    .multilineTextAlignment(.center)
                    .font(.body)

                // Key capture area
                Text(capturedKeys)
                    .frame(width: 240, height: 40)
                    .background(isCapturing ? Color.accentColor.opacity(0.1) : Color(NSColor.controlBackgroundColor))
                    .overlay(
                        RoundedRectangle(cornerRadius: 8)
                            .stroke(isCapturing ? Color.accentColor : Color(NSColor.separatorColor), lineWidth: 1)
                    )
                    .cornerRadius(8)
                    .onTapGesture {
                        startCapturing()
                    }

                // Toggle mode checkbox
                HStack {
                    Toggle("Push to toggle (press once to start, once to stop)", isOn: $pushToToggle)
                        .font(.body)
                    Spacer()
                }
            }
            .padding(.horizontal, 20)
            .padding(.bottom, 16)

            // Bottom buttons
            HStack(spacing: 12) {
                Spacer()
                
                Button("Cancel") {
                    stopCapturing()
                    state.dialogResult = false
                    state.isCompleted = true
                }
                .controlSize(.large)

                Button("OK") {
                    stopCapturing()
                    if hasValidCombination {
                        state.capturedCombination = capturedCombination
                        // Save toggle mode preference
                        saveToggleMode()
                        state.dialogResult = true
                    } else {
                        state.dialogResult = false
                    }
                    state.isCompleted = true
                }
                .disabled(!hasValidCombination)
                .controlSize(.large)
                .keyboardShortcut(.defaultAction)
            }
            .padding(.horizontal, 20)
            .padding(.bottom, 20)
        }
        .frame(width: 400)
        .background(Color(NSColor.windowBackgroundColor))
        .background(KeyEventHandler(
            onKeyDown: { event in
                if isCapturing {
                    handleKeyDown(event)
                }
            }
        ))
        .onAppear {
            loadCurrentHotkey()
            loadToggleMode()
        }
    }
    
    private func loadCurrentHotkey() {
        // Load the current hotkey from preferences
        var combo = KeyCombination(keys: (KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0)), count: 0)
        
        // Create a buffer to match the C struct layout
        var buffer = [UInt32](repeating: 0, count: 9) // 8 for keys + 1 for count
        let success = preferences_load_key_combination(&buffer)
        
        if success {
            // Parse the loaded combination
            combo = KeyCombination(
                keys: (
                    KeyInfo(code: buffer[0], flags: buffer[1]),
                    KeyInfo(code: buffer[2], flags: buffer[3]),
                    KeyInfo(code: buffer[4], flags: buffer[5]),
                    KeyInfo(code: buffer[6], flags: buffer[7])
                ),
                count: Int32(buffer[8])
            )
            
            // Format the combination for display
            capturedKeys = formatKeyComboForDisplay(combo)
            capturedCombination = combo
            hasValidCombination = true
        } else {
            // No hotkey set
            capturedKeys = "No hotkey set"
            hasValidCombination = false
        }
    }
    
    private func loadToggleMode() {
        pushToToggle = "push_to_toggle".withCString { key in
            preferences_get_bool(key, false)
        }
    }
    
    private func saveToggleMode() {
        "push_to_toggle".withCString { key in
            preferences_set_bool(key, pushToToggle)
        }
        _ = preferences_save()
    }
    
    private func formatKeyComboForDisplay(_ combo: KeyCombination) -> String {
        var displayKeys: [String] = []
        
        // Process each key in the combination
        for i in 0..<min(Int(combo.count), 4) {
            let key = [combo.keys.0, combo.keys.1, combo.keys.2, combo.keys.3][i]
            if key.code == 0 && key.flags == 0 { continue }
            
            var keyDisplayParts: [String] = []
            
            // Add modifier symbols based on flags
            let flags = NSEvent.ModifierFlags(rawValue: UInt(convertFromCGEventFlags(key.flags)))
            if flags.contains(.control) { keyDisplayParts.append("⌃") }
            if flags.contains(.option) { keyDisplayParts.append("⌥") }
            if flags.contains(.shift) { keyDisplayParts.append("⇧") }
            if flags.contains(.command) { keyDisplayParts.append("⌘") }
            if flags.contains(.function) { keyDisplayParts.append("fn") }
            if flags.contains(.capsLock) { keyDisplayParts.append("⇪") }
            
            // Add the key itself (if it's not just modifiers)
            if key.code != 0 {
                keyDisplayParts.append(getKeyDisplayName(UInt16(key.code)))
            }
            
            if !keyDisplayParts.isEmpty {
                displayKeys.append(keyDisplayParts.joined(separator: ""))
            }
        }
        
        return displayKeys.isEmpty ? "No hotkey set" : displayKeys.joined(separator: " + ")
    }
    
    // Convert CGEvent flags back to NSEvent modifier flags
    private func convertFromCGEventFlags(_ cgFlags: UInt32) -> UInt {
        var nsFlags: UInt = 0
        
        if cgFlags & 0x40000 != 0 { nsFlags |= NSEvent.ModifierFlags.control.rawValue }    // kCGEventFlagMaskControl
        if cgFlags & 0x80000 != 0 { nsFlags |= NSEvent.ModifierFlags.option.rawValue }     // kCGEventFlagMaskAlternate
        if cgFlags & 0x20000 != 0 { nsFlags |= NSEvent.ModifierFlags.shift.rawValue }      // kCGEventFlagMaskShift
        if cgFlags & 0x100000 != 0 { nsFlags |= NSEvent.ModifierFlags.command.rawValue }   // kCGEventFlagMaskCommand
        if cgFlags & 0x800000 != 0 { nsFlags |= NSEvent.ModifierFlags.function.rawValue }  // kCGEventFlagMaskSecondaryFn
        if cgFlags & 0x10000 != 0 { nsFlags |= NSEvent.ModifierFlags.capsLock.rawValue }   // kCGEventFlagMaskAlphaShift
        
        return nsFlags
    }

    private func startCapturing() {
        isCapturing = true
        capturedKeys = "Press keys..."
        hasValidCombination = false
        capturedCombination = KeyCombination(keys: (KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0)), count: 0)
        lastFlags = []
        currentKeys = []
        currentModifiers = []
        allPressedKeys = []
        maxModifiers = []

        // Pause keylogger while capturing (if available)
        keylogger_pause()
    }

    private func stopCapturing() {
        if isCapturing {
            isCapturing = false
            keylogger_resume()
        }
    }

    private func handleKeyDown(_ event: NSEvent) {
        if !isCapturing { return }

        if event.type == .keyDown {
            // Add this key to our current combination and track all pressed keys
            let keyCode = UInt16(event.keyCode)
            currentKeys.insert(keyCode)
            allPressedKeys.insert(keyCode)
            currentModifiers = event.modifierFlags

            // Track the maximum modifiers seen during this session
            maxModifiers.formUnion(currentModifiers)

            // Update display to show current combination
            updateDisplayString()

        } else if event.type == .keyUp {
            // Remove this key from our current combination
            let keyCode = UInt16(event.keyCode)
            currentKeys.remove(keyCode)

            // Check if all keys and modifiers are released
            checkForCompletion()

        } else if event.type == .flagsChanged {
            let flags = event.modifierFlags
            currentModifiers = flags

            // Track the maximum modifiers seen during this session
            maxModifiers.formUnion(flags)

            // Check if all keys and relevant modifiers are released BEFORE updating display
            let relevantModifiers: NSEvent.ModifierFlags = [.control, .option, .shift, .command, .function, .capsLock]
            let currentRelevantModifiers = currentModifiers.intersection(relevantModifiers)
            if currentKeys.isEmpty && currentRelevantModifiers.rawValue == 0 && (maxModifiers.rawValue != 0 || !allPressedKeys.isEmpty) {
                finalizeCombination()
                return
            }

            // Update display to show current combination
            updateDisplayString()

            lastFlags = flags
        }
    }

    private func updateDisplayString() {
        var displayKeys: [String] = []

        // Add modifier symbols
        if currentModifiers.contains(.control) { displayKeys.append("⌃") }
        if currentModifiers.contains(.option) { displayKeys.append("⌥") }
        if currentModifiers.contains(.shift) { displayKeys.append("⇧") }
        if currentModifiers.contains(.command) { displayKeys.append("⌘") }
        if currentModifiers.contains(.function) { displayKeys.append("fn") }
        if currentModifiers.contains(.capsLock) { displayKeys.append("⇪") }

        // Add all pressed keys
        for keyCode in currentKeys.sorted() {
            displayKeys.append(getKeyDisplayName(keyCode))
        }

        capturedKeys = displayKeys.isEmpty ? "Press keys..." : displayKeys.joined(separator: " + ")
    }

    private func checkForCompletion() {
        // Only finalize when we have captured something AND nothing relevant is currently pressed
        let relevantModifiers: NSEvent.ModifierFlags = [.control, .option, .shift, .command, .function, .capsLock]
        let currentRelevantModifiers = currentModifiers.intersection(relevantModifiers)
        if currentKeys.isEmpty && currentRelevantModifiers.rawValue == 0 && (maxModifiers.rawValue != 0 || !allPressedKeys.isEmpty) {
            finalizeCombination()
        }
    }

    private func finalizeCombination() {
        // Build the final combination from ALL keys that were pressed during the session
        var finalKeys: [KeyInfo] = []

        if maxModifiers.rawValue != 0 {
            // Create entries for each key with the modifiers that were seen
            if allPressedKeys.isEmpty {
                // Modifier-only combination
                finalKeys.append(KeyInfo(code: 0, flags: convertToCGEventFlags(maxModifiers)))
            } else {
                // Each key gets the modifiers
                for keyCode in allPressedKeys.sorted() {
                    finalKeys.append(KeyInfo(code: UInt32(keyCode), flags: convertToCGEventFlags(maxModifiers)))
                }
            }
        } else {
            // No modifiers, just the keys
            for keyCode in allPressedKeys.sorted() {
                finalKeys.append(KeyInfo(code: UInt32(keyCode), flags: 0))
            }
        }

        if !finalKeys.isEmpty || !allPressedKeys.isEmpty || maxModifiers.rawValue != 0 {
            // Pad the keys array to 4 elements
            while finalKeys.count < 4 {
                finalKeys.append(KeyInfo(code: 0, flags: 0))
            }

            capturedCombination = KeyCombination(
                keys: (finalKeys[0], finalKeys[1], finalKeys[2], finalKeys[3]),
                count: 1
            )
            hasValidCombination = true

            // Build final display string from all pressed keys
            var displayKeys: [String] = []
            if maxModifiers.contains(.control) { displayKeys.append("⌃") }
            if maxModifiers.contains(.option) { displayKeys.append("⌥") }
            if maxModifiers.contains(.shift) { displayKeys.append("⇧") }
            if maxModifiers.contains(.command) { displayKeys.append("⌘") }
            if maxModifiers.contains(.function) { displayKeys.append("fn") }
            if maxModifiers.contains(.capsLock) { displayKeys.append("⇪") }

            for keyCode in allPressedKeys.sorted() {
                displayKeys.append(getKeyDisplayName(keyCode))
            }

            capturedKeys = displayKeys.joined(separator: " + ")
            isCapturing = false
            keylogger_resume()
        }
    }


    // Convert NSEvent modifier flags to CGEvent flags
    private func convertToCGEventFlags(_ nsFlags: NSEvent.ModifierFlags) -> UInt32 {
        var cgFlags: UInt32 = 0

        if nsFlags.contains(.control) { cgFlags |= 0x40000 } // kCGEventFlagMaskControl
        if nsFlags.contains(.option) { cgFlags |= 0x80000 } // kCGEventFlagMaskAlternate
        if nsFlags.contains(.shift) { cgFlags |= 0x20000 } // kCGEventFlagMaskShift
        if nsFlags.contains(.command) { cgFlags |= 0x100000 } // kCGEventFlagMaskCommand
        if nsFlags.contains(.function) { cgFlags |= 0x800000 } // kCGEventFlagMaskSecondaryFn
        if nsFlags.contains(.capsLock) { cgFlags |= 0x10000 } // kCGEventFlagMaskAlphaShift

        return cgFlags
    }

    private func isSpecialKey(_ keyCode: UInt16) -> Bool {
        // Function keys F1-F12
        let functionKeys: [UInt16] = [122, 120, 99, 118, 96, 97, 98, 100, 101, 109, 111]
        if functionKeys.contains(keyCode) { return true }

        // Arrow keys
        let arrowKeys: [UInt16] = [126, 125, 123, 124]
        if arrowKeys.contains(keyCode) { return true }

        return false
    }

    private func getKeyDisplayName(_ keyCode: UInt16) -> String {
        switch keyCode {
        // Function keys
        case 122: return "F1"
        case 120: return "F2"
        case 99: return "F3"
        case 118: return "F4"
        case 96: return "F5"
        case 97: return "F6"
        case 98: return "F7"
        case 100: return "F8"
        case 101: return "F9"
        case 109: return "F10"
        case 111: return "F12"
        // Arrow keys
        case 126: return "↑"
        case 125: return "↓"
        case 123: return "←"
        case 124: return "→"
        // Special keys
        case 51: return "⌫" // Delete
        case 36: return "↩" // Return
        case 76: return "↩" // Enter (numpad)
        case 48: return "⇥" // Tab
        case 53: return "⎋" // Escape
        case 49: return "Space"
        // Numbers
        case 29: return "0"
        case 18: return "1"
        case 19: return "2"
        case 20: return "3"
        case 21: return "4"
        case 23: return "5"
        case 22: return "6"
        case 26: return "7"
        case 28: return "8"
        case 25: return "9"
        // Letters
        case 0: return "A"
        case 11: return "B"
        case 8: return "C"
        case 2: return "D"
        case 14: return "E"
        case 3: return "F"
        case 5: return "G"
        case 4: return "H"
        case 34: return "I"
        case 38: return "J"
        case 40: return "K"
        case 37: return "L"
        case 46: return "M"
        case 45: return "N"
        case 31: return "O"
        case 35: return "P"
        case 12: return "Q"
        case 15: return "R"
        case 1: return "S"
        case 17: return "T"
        case 32: return "U"
        case 9: return "V"
        case 13: return "W"
        case 7: return "X"
        case 16: return "Y"
        case 6: return "Z"
        // Punctuation and symbols
        case 27: return "-"
        case 24: return "="
        case 33: return "["
        case 30: return "]"
        case 42: return "\\"
        case 41: return ";"
        case 39: return "'"
        case 50: return "`"
        case 43: return ","
        case 47: return "."
        case 44: return "/"
        // Additional keys that might include ^
        case 10: return "§" // Section sign (varies by layout)
        default: return "Key\(keyCode)"
        }
    }
}

// MARK: - Key Event Handler
struct KeyEventHandler: NSViewRepresentable {
    let onKeyDown: (NSEvent) -> Void

    func makeNSView(context: Context) -> NSView {
        let view = KeyCaptureView()
        view.onKeyDown = onKeyDown
        return view
    }

    func updateNSView(_ nsView: NSView, context: Context) {}
}

class KeyCaptureView: NSView {
    var onKeyDown: ((NSEvent) -> Void)?

    override var acceptsFirstResponder: Bool { true }
    override var canBecomeKeyView: Bool { true }

    override func keyDown(with event: NSEvent) {
        onKeyDown?(event)
    }

    override func keyUp(with event: NSEvent) {
        // Handle key up events for potential multi-key combinations
        onKeyDown?(event)
    }

    override func flagsChanged(with event: NSEvent) {
        // Handle modifier-only events (like Fn key)
        onKeyDown?(event)
    }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        window?.makeFirstResponder(self)
    }
}

// MARK: - Data Types for C Interop
struct KeyInfo {
    let code: UInt32
    let flags: UInt32
}

struct KeyCombination {
    let keys: (KeyInfo, KeyInfo, KeyInfo, KeyInfo)
    let count: Int32
}

// MARK: - Shared Dialog State
class DialogState: ObservableObject {
    @Published var isCompleted = false
    @Published var dialogResult = false
    var capturedCombination = KeyCombination(keys: (KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0)), count: 0)
}

// MARK: - ModalDialogState Conformance
extension DialogState: ModalDialogState {
    typealias ResultType = Bool

    var result: Bool {
        return dialogResult
    }

    func reset() {
        isCompleted = false
        dialogResult = false
        capturedCombination = KeyCombination(keys: (KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0)), count: 0)
    }
}

private var globalDialogState = DialogState()


// MARK: - Keylogger Function Declarations
@_silgen_name("keylogger_pause")
func keylogger_pause()

@_silgen_name("keylogger_resume")
func keylogger_resume()

@_silgen_name("preferences_load_key_combination")
func preferences_load_key_combination(_ combo: UnsafeMutableRawPointer) -> Bool

@_silgen_name("preferences_get_bool")
func preferences_get_bool(_ key: UnsafePointer<CChar>, _ defaultValue: Bool) -> Bool

@_silgen_name("preferences_set_bool")
func preferences_set_bool(_ key: UnsafePointer<CChar>, _ value: Bool)

@_silgen_name("preferences_save")
func preferences_save() -> Bool

// MARK: - C Interface Function
@_cdecl("dialog_keycombination_capture")
public func dialog_keycombination_capture(
    _ title: UnsafePointer<CChar>,
    _ message: UnsafePointer<CChar>,
    _ result: UnsafeMutableRawPointer
) -> Bool {

    let titleStr = String(cString: title)
    let messageStr = String(cString: message)

    globalDialogState.capturedCombination = KeyCombination(keys: (KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0), KeyInfo(code: 0, flags: 0)), count: 0)

    let success = runModalDialog(
        content: HotkeyDialogView(state: globalDialogState, title: titleStr, message: messageStr),
        state: globalDialogState,
        windowSize: NSSize(width: 400, height: 340),
        windowTitle: "Key Combination Capture"
    )

    if success {
        // Copy the captured combination to the C struct
        let combo = globalDialogState.capturedCombination

        // Manually write to the raw pointer matching KeyCombination C struct layout
        let resultPtr = result.assumingMemoryBound(to: UInt32.self)
        resultPtr[0] = combo.keys.0.code    // keys[0].code
        resultPtr[1] = combo.keys.0.flags   // keys[0].flags
        resultPtr[2] = combo.keys.1.code    // keys[1].code
        resultPtr[3] = combo.keys.1.flags   // keys[1].flags
        resultPtr[4] = combo.keys.2.code    // keys[2].code
        resultPtr[5] = combo.keys.2.flags   // keys[2].flags
        resultPtr[6] = combo.keys.3.code    // keys[3].code
        resultPtr[7] = combo.keys.3.flags   // keys[3].flags
        resultPtr[8] = UInt32(combo.count)  // count
    }

    return success
}