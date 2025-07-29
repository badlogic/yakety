import SwiftUI
import AppKit
import Foundation

// MARK: - C Function Declarations
@_silgen_name("preferences_get_string")
func preferences_get_string(_ key: UnsafePointer<CChar>) -> UnsafePointer<CChar>?

// MARK: - Data Models
struct ModelItem: Identifiable {
    let id = UUID()
    let name: String
    let description: String
    let size: String
    let filename: String
    let downloadUrl: String
    let isBundled: Bool
    let isInstalled: Bool
    let path: String
}

struct LanguageItem: Identifiable, Hashable {
    let id = UUID()
    let name: String
    let code: String
    
    static func == (lhs: LanguageItem, rhs: LanguageItem) -> Bool {
        return lhs.code == rhs.code
    }
    
    func hash(into hasher: inout Hasher) {
        hasher.combine(code)
    }
}

// MARK: - Models Dialog
struct ModelsDialogView: View {
    @ObservedObject var state: ModelsDialogState
    @State private var selectedLanguage: LanguageItem
    @State private var selectedModel: ModelItem?
    @State private var models: [ModelItem] = []
    @State private var languages: [LanguageItem] = []
    @State private var showingDeleteConfirmation = false
    @State private var modelToDelete: ModelItem?
    
    init(state: ModelsDialogState) {
        self.state = state
        self._selectedLanguage = State(initialValue: LanguageItem(name: "Auto Detection", code: "auto"))
    }
    
    var body: some View {
        VStack(spacing: 0) {
            // Header with icon and title
            HStack(spacing: 12) {
                if let appIcon = loadAppIcon() {
                    Image(nsImage: appIcon)
                        .resizable()
                        .frame(width: 32, height: 32)
                }
                
                Text("Select Model and Language")
                    .font(.title2)
                    .fontWeight(.medium)
                
                Spacer()
            }
            .padding(.horizontal, 20)
            .padding(.top, 20)
            .padding(.bottom, 24)
            
            // Content area
            VStack(spacing: 20) {
                // Language Selection
                VStack(alignment: .leading, spacing: 8) {
                    HStack {
                        Text("Language")
                            .font(.headline)
                            .fontWeight(.medium)
                        
                        Picker("", selection: $selectedLanguage) {
                            ForEach(languages) { language in
                                Text(language.name).tag(language)
                            }
                        }
                        .pickerStyle(MenuPickerStyle())
                        
                        Spacer()
                    }
                    
                    Text("Language detection can add minor latency")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                
                // Models Section
                VStack(alignment: .leading, spacing: 12) {
                    Text("Model")
                        .font(.headline)
                        .fontWeight(.medium)
                    
                    ScrollView {
                        LazyVStack(spacing: 8) {
                            ForEach(models) { model in
                                ModelRowView(
                                    model: model,
                                    isSelected: selectedModel?.id == model.id,
                                    onSelect: { selectedModel = model },
                                    onDelete: { model in
                                        modelToDelete = model
                                        showingDeleteConfirmation = true
                                    }
                                )
                            }
                        }
                        .padding(.horizontal, 8)
                        .padding(.vertical, 4)
                    }
                    .frame(height: 278)
                }
                .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(.horizontal, 20)
            .padding(.bottom, 16)
            
            // Bottom buttons
            HStack(spacing: 12) {
                Spacer()
                
                Button("Cancel") {
                    state.dialogResult = false
                    state.isCompleted = true
                }
                .controlSize(.large)
                
                Button("Apply") {
                    if let model = selectedModel {
                        state.selectedModelPath = model.isBundled ? "" : model.path
                        state.selectedLanguageCode = selectedLanguage.code
                        // Only set download URL if model needs to be downloaded
                        state.selectedDownloadUrl = model.isInstalled ? "" : model.downloadUrl
                    }
                    state.dialogResult = true
                    state.isCompleted = true
                }
                .disabled(selectedModel == nil)
                .controlSize(.large)
                .keyboardShortcut(.defaultAction)
            }
            .padding(.horizontal, 20)
            .padding(.bottom, 20)
        }
        .frame(width: 520)
        .background(Color(NSColor.windowBackgroundColor))
        .alert("Delete Model", isPresented: $showingDeleteConfirmation) {
            Button("Cancel", role: .cancel) { }
            Button("Delete", role: .destructive) {
                if let model = modelToDelete {
                    deleteModel(model)
                }
            }
        } message: {
            if let model = modelToDelete {
                Text("Are you sure you want to delete \"\(model.name)\"? This action cannot be undone.")
            }
        }
        .onAppear {
            loadData()
            restoreModelSelection()
            restoreLanguageSelection()
        }
    }
    
    private func loadData() {
        // Load languages
        languages = [
            LanguageItem(name: "Auto Detection", code: "auto"),
            LanguageItem(name: "English", code: "en"),
            LanguageItem(name: "Spanish", code: "es"),
            LanguageItem(name: "French", code: "fr"),
            LanguageItem(name: "German", code: "de"),
            LanguageItem(name: "Italian", code: "it"),
            LanguageItem(name: "Portuguese", code: "pt"),
            LanguageItem(name: "Chinese", code: "zh"),
            LanguageItem(name: "Japanese", code: "ja"),
            LanguageItem(name: "Korean", code: "ko"),
            LanguageItem(name: "Russian", code: "ru")
        ]
        
        // Load models
        let homeDir = FileManager.default.homeDirectoryForCurrentUser.path
        let modelsDir = "\(homeDir)/.yakety/models"
        
        var modelsList: [ModelItem] = []
        
        // Bundled model
        modelsList.append(ModelItem(
            name: "Whisper Base Q8",
            description: "Bundled model optimized for real-time transcription",
            size: "75 MB",
            filename: "",
            downloadUrl: "",
            isBundled: true,
            isInstalled: true,
            path: ""
        ))
        
        // Downloadable models
        let downloadableModels = [
            (name: "Whisper Tiny Q8", desc: "Ultra-lightweight model for basic transcription", size: "40 MB", filename: "ggml-tiny-q8_0.bin", url: "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny-q8_0.bin"),
            (name: "Whisper Large-v3 Turbo Q8", desc: "Premium model delivering the highest accuracy", size: "800 MB", filename: "ggml-large-v3-turbo-q8_0.bin", url: "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q8_0.bin"),
            (name: "Whisper Large-v3 Turbo Q5", desc: "Faster premium model with slightly reduced accuracy", size: "550 MB", filename: "ggml-large-v3-turbo-q5_0.bin", url: "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin")
        ]
        
        for model in downloadableModels {
            let path = "\(modelsDir)/\(model.filename)"
            modelsList.append(ModelItem(
                name: model.name,
                description: model.desc,
                size: model.size,
                filename: model.filename,
                downloadUrl: model.url,
                isBundled: false,
                isInstalled: FileManager.default.fileExists(atPath: path),
                path: path
            ))
        }
        
        models = modelsList
    }
    
    private func restoreModelSelection() {
        // Get current model path from preferences (same as old dialog)
        let currentModelPath = getCurrentModelPath()
        
        // Find the model that matches the current path
        var foundModel: ModelItem? = nil
        
        for model in models {
            var isCurrentModel = false
            if model.path.isEmpty {
                // Bundled model - matches if current path is empty
                isCurrentModel = currentModelPath.isEmpty
            } else {
                // File model - exact path match
                isCurrentModel = model.path == currentModelPath
            }
            
            if isCurrentModel {
                foundModel = model
                break
            }
        }
        
        // Set the selected model
        if let model = foundModel {
            selectedModel = model
            state.selectedModelPath = model.path
        } else if let firstModel = models.first {
            // Fallback to first model if current not found
            selectedModel = firstModel
            state.selectedModelPath = firstModel.path
        }
    }
    
    private func getCurrentModelPath() -> String {
        // Call the same C function that the old dialog uses
        let cString = preferences_get_string("model")
        if let cString = cString, strlen(cString) > 0 {
            return String(cString: cString)
        }
        return "" // Empty string represents bundled model
    }
    
    private func restoreLanguageSelection() {
        // Get current language from preferences (same as old dialog)
        let cString = preferences_get_string("language")
        let currentLanguageCode = (cString != nil && strlen(cString!) > 0) ? String(cString: cString!) : "en"
        
        // Find and select the matching language
        for language in languages {
            if language.code == currentLanguageCode {
                selectedLanguage = language
                return
            }
        }
        
        // Fallback to English if not found
        if let englishLang = languages.first(where: { $0.code == "en" }) {
            selectedLanguage = englishLang
        }
    }
    
    private func deleteModel(_ model: ModelItem) {
        guard !model.isBundled && model.isInstalled else { return }
        
        do {
            try FileManager.default.removeItem(atPath: model.path)
            // Refresh the models list to update the UI
            loadData()
            // If the deleted model was selected, clear selection
            if selectedModel?.id == model.id {
                selectedModel = models.first
            }
        } catch {
            // Handle deletion error - you might want to show an alert here
            print("Failed to delete model: \(error)")
        }
    }
}

struct ModelRowView: View {
    let model: ModelItem
    let isSelected: Bool
    let onSelect: () -> Void
    let onDelete: (ModelItem) -> Void
    
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(model.name)
                    .font(.headline)
                    .foregroundColor(.primary)
                Spacer()
                
                if model.isBundled {
                    Text("Bundled")
                        .foregroundColor(.blue)
                        .font(.caption)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 2)
                        .background(Color.blue.opacity(0.1))
                        .cornerRadius(4)
                } else if model.isInstalled {
                    HStack(spacing: 8) {
                        Text("Installed")
                            .foregroundColor(.green)
                            .font(.caption)
                            .padding(.horizontal, 8)
                            .padding(.vertical, 2)
                            .background(Color.green.opacity(0.1))
                            .cornerRadius(4)
                        
                        Button(action: {
                            onDelete(model)
                        }) {
                            Image(systemName: "trash")
                                .font(.caption)
                                .foregroundColor(.red)
                        }
                        .buttonStyle(PlainButtonStyle())
                        .padding(4)
                        .background(Color.red.opacity(0.1))
                        .cornerRadius(4)
                        .help("Delete this model")
                    }
                } else {
                    Text("Download Required")
                        .foregroundColor(.orange)
                        .font(.caption)
                        .padding(.horizontal, 8)
                        .padding(.vertical, 2)
                        .background(Color.orange.opacity(0.1))
                        .cornerRadius(4)
                }
            }
            
            Text(model.description)
                .font(.body)
                .foregroundColor(.secondary)
                .multilineTextAlignment(.leading)
            
            Text("Size: \(model.size)")
                .font(.caption)
                .foregroundColor(.secondary)
        }
        .padding(16)
        .background(
            RoundedRectangle(cornerRadius: 12)
                .fill(Color(NSColor.controlBackgroundColor))
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(isSelected ? Color.accentColor : Color.clear, lineWidth: 2)
                )
        )
        .onTapGesture {
            onSelect()
        }
        .scaleEffect(isSelected ? 1.02 : 1.0)
        .animation(.easeInOut(duration: 0.1), value: isSelected)
    }
}

// MARK: - Dialog State
class ModelsDialogState: ObservableObject {
    @Published var isCompleted = false
    @Published var dialogResult = false
    @Published var selectedModelPath = ""
    @Published var selectedLanguageCode = ""
    @Published var selectedDownloadUrl = ""
    @Published var selectedModelName = "" // Track by name for persistence
}

// MARK: - ModalDialogState Conformance
extension ModelsDialogState: ModalDialogState {
    typealias ResultType = Bool
    
    var result: Bool {
        return dialogResult
    }
    
    func reset() {
        isCompleted = false
        dialogResult = false
        selectedModelPath = ""
        selectedLanguageCode = ""
        selectedDownloadUrl = ""
    }
}

private var globalModelsDialogState = ModelsDialogState()


// MARK: - C Interface Function
@_cdecl("dialog_models_and_language")
public func dialog_models_and_language(
    _ title: UnsafePointer<CChar>,
    _ selected_model: UnsafeMutablePointer<CChar>,
    _ model_buffer_size: Int,
    _ selected_language: UnsafeMutablePointer<CChar>,
    _ language_buffer_size: Int,
    _ download_url: UnsafeMutablePointer<CChar>,
    _ url_buffer_size: Int
) -> Bool {
    
    let result = runModalDialog(
        content: ModelsDialogView(state: globalModelsDialogState),
        state: globalModelsDialogState,
        windowSize: NSSize(width: 520, height: 400),
        windowTitle: "Model and Language Selection"
    )
    
    if result {
        // Copy results back to C strings
        let modelPath = globalModelsDialogState.selectedModelPath
        let languageCode = globalModelsDialogState.selectedLanguageCode
        let downloadUrlStr = globalModelsDialogState.selectedDownloadUrl
        
        strncpy(selected_model, modelPath, model_buffer_size - 1)
        selected_model[model_buffer_size - 1] = 0
        
        strncpy(selected_language, languageCode, language_buffer_size - 1)
        selected_language[language_buffer_size - 1] = 0
        
        strncpy(download_url, downloadUrlStr, url_buffer_size - 1)
        download_url[url_buffer_size - 1] = 0
    }
    
    return result
}