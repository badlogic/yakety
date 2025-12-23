import SwiftUI
import AppKit
import Foundation
import os.log

// MARK: - C Logging Functions
@_silgen_name("log_info")
func c_log_info(_ format: UnsafePointer<CChar>, _ args: CVarArg...)

@_silgen_name("log_error")
func c_log_error(_ format: UnsafePointer<CChar>, _ args: CVarArg...)

@_silgen_name("log_debug")
func c_log_debug(_ format: UnsafePointer<CChar>, _ args: CVarArg...)

// Swift logging wrappers - use os_log for safety
let log = OSLog(subsystem: "com.yakety.app", category: "Download")

func logInfo(_ message: String) {
    os_log("%{public}@", log: log, type: .info, message)
}

func logError(_ message: String) {
    os_log("%{public}@", log: log, type: .error, message)
}

func logDebug(_ message: String) {
    os_log("%{public}@", log: log, type: .debug, message)
}
// MARK: - Download Dialog
struct DownloadDialogView: View {
    @ObservedObject var state: DownloadDialogState
    let modelName: String
    let downloadUrl: String
    let filePath: String
    
    var body: some View {
        VStack(spacing: 0) {
            // Header with icon and title
            HStack(spacing: 12) {
                if let appIcon = loadAppIcon() {
                    Image(nsImage: appIcon)
                        .resizable()
                        .frame(width: 32, height: 32)
                }
                
                Text("Downloading \(modelName)")
                    .font(.title2)
                    .fontWeight(.medium)
                
                Spacer()
            }
            .padding(.horizontal, 20)
            .padding(.top, 20)
            .padding(.bottom, 24)
            
            // Content area
            VStack(spacing: 16) {
                ProgressView(value: state.downloadProgress)
                    .progressViewStyle(LinearProgressViewStyle())
                    .scaleEffect(x: 1, y: 1.5)
                
                Text(state.downloadStatus)
                    .foregroundColor(.secondary)
                    .font(.body)
            }
            .padding(.horizontal, 20)
            .padding(.bottom, 16)
            
            // Bottom buttons
            HStack(spacing: 12) {
                Spacer()
                
                Button("Cancel") {
                    state.downloadResult = 1 // Cancelled
                    state.isCompleted = true
                }
                .disabled(state.downloadProgress >= 1.0)
                .controlSize(.large)
            }
            .padding(.horizontal, 20)
            .padding(.bottom, 20)
        }
        .frame(width: 400)
        .background(Color(NSColor.windowBackgroundColor))
        .onAppear {
            startDownload()
        }
    }
    
    private func startDownload() {
        logInfo("Starting download for model: \(modelName)")
        logInfo("Download URL: \(downloadUrl)")
        logInfo("Destination path: \(filePath)")
        
        // Create the directory if it doesn't exist
        let directory = (filePath as NSString).deletingLastPathComponent
        do {
            try FileManager.default.createDirectory(atPath: directory, withIntermediateDirectories: true, attributes: nil)
            logInfo("Created directory: \(directory)")
        } catch {
            logError("Failed to create directory: \(error.localizedDescription)")
        }
        
        guard let url = URL(string: downloadUrl) else {
            logError("Invalid URL: \(downloadUrl)")
            state.downloadStatus = "Invalid URL"
            state.downloadResult = 2 // Error
            state.isCompleted = true
            return
        }
        
        let destinationURL = URL(fileURLWithPath: filePath)
        
        // Use URLSession to download
        let task = URLSession.shared.downloadTask(with: url) { tempURL, response, error in
            DispatchQueue.main.async {
                if let error = error {
                    logError("Download failed with error: \(error.localizedDescription)")
                    logError("Error details: \(String(describing: error))")
                    state.downloadStatus = "Download failed: \(error.localizedDescription)"
                    state.downloadResult = 2 // Error
                    state.isCompleted = true
                    return
                }
                
                // Log response details
                if let httpResponse = response as? HTTPURLResponse {
                    logInfo("HTTP response code: \(httpResponse.statusCode)")
                    logInfo("Content-Length: \(httpResponse.expectedContentLength)")
                }
                
                guard let tempURL = tempURL else {
                    logError("Download failed: No temporary URL received")
                    state.downloadStatus = "Download failed: No data received"
                    state.downloadResult = 2 // Error
                    state.isCompleted = true
                    return
                }
                
                logInfo("Download completed to temporary location: \(tempURL.path)")
                
                do {
                    // Check file size at temp location
                    let tempAttributes = try FileManager.default.attributesOfItem(atPath: tempURL.path)
                    let fileSize = tempAttributes[.size] as? Int64 ?? 0
                    logInfo("Downloaded file size: \(fileSize) bytes")
                    
                    // Remove existing file if it exists
                    if FileManager.default.fileExists(atPath: filePath) {
                        logInfo("Removing existing file at: \(filePath)")
                        try FileManager.default.removeItem(at: destinationURL)
                    }
                    
                    // Move downloaded file to final location
                    logInfo("Moving file from \(tempURL.path) to \(destinationURL.path)")
                    try FileManager.default.moveItem(at: tempURL, to: destinationURL)
                    
                    // Verify file at final location
                    let finalAttributes = try FileManager.default.attributesOfItem(atPath: destinationURL.path)
                    let finalSize = finalAttributes[.size] as? Int64 ?? 0
                    logInfo("Final file size: \(finalSize) bytes")
                    
                    state.downloadStatus = "Download complete"
                    state.downloadProgress = 1.0
                    state.downloadResult = 0 // Success
                    state.isCompleted = true
                    logInfo("Download completed successfully")
                } catch {
                    logError("Failed to save file: \(error.localizedDescription)")
                    logError("Error details: \(String(describing: error))")
                    state.downloadStatus = "Failed to save file: \(error.localizedDescription)"
                    state.downloadResult = 2 // Error
                    state.isCompleted = true
                }
            }
        }
        
        // Monitor download progress
        let observation = task.progress.observe(\.fractionCompleted) { progress, _ in
            DispatchQueue.main.async {
                state.downloadProgress = progress.fractionCompleted
                let percentage = Int(progress.fractionCompleted * 100)
                state.downloadStatus = "Downloaded \(percentage)%"
                
                // Log progress milestones
                if percentage == 25 || percentage == 50 || percentage == 75 || percentage == 100 {
                    logInfo("Download progress: \(percentage)%")
                }
            }
        }
        
        // Start the download
        logInfo("Starting download task...")
        task.resume()
        
        // Keep the observation alive
        state.progressObservation = observation
    }
}

// MARK: - Dialog State
class DownloadDialogState: ObservableObject {
    @Published var isCompleted = false
    @Published var downloadResult = 0 // 0 = success, 1 = cancelled, 2 = error
    @Published var downloadProgress: Double = 0
    @Published var downloadStatus = "Starting download..."
    var progressObservation: NSKeyValueObservation?
    
    deinit {
        progressObservation?.invalidate()
    }
}

// MARK: - ModalDialogState Conformance
extension DownloadDialogState: ModalDialogState {
    typealias ResultType = Int32
    
    var result: Int32 {
        return Int32(downloadResult)
    }
    
    func reset() {
        isCompleted = false
        downloadResult = 0
        downloadProgress = 0
        downloadStatus = "Starting download..."
        progressObservation?.invalidate()
        progressObservation = nil
    }
}

private var globalDownloadDialogState = DownloadDialogState()


// MARK: - C Interface Function
@_cdecl("dialog_model_download")
public func dialog_model_download(
    _ model_name: UnsafePointer<CChar>,
    _ download_url: UnsafePointer<CChar>,
    _ file_path: UnsafePointer<CChar>
) -> Int32 {
    
    let modelNameStr = String(cString: model_name)
    let downloadUrlStr = String(cString: download_url)
    let filePathStr = String(cString: file_path)
    
    return runModalDialog(
        content: DownloadDialogView(
            state: globalDownloadDialogState,
            modelName: modelNameStr,
            downloadUrl: downloadUrlStr,
            filePath: filePathStr
        ),
        state: globalDownloadDialogState,
        windowSize: NSSize(width: 400, height: 200),
        windowTitle: "Model Download"
    )
}