#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>

// Audio recording configuration
typedef struct {
    int sample_rate;          // Sample rate (e.g., 16000, 44100)
    int channels;            // Number of channels (1 = mono, 2 = stereo)
    int bits_per_sample;     // Bits per sample (16, 24, 32)
} AudioConfig;

// Audio recording session handle
typedef struct AudioRecorder AudioRecorder;

// Default audio configuration for Whisper (16kHz mono)
extern const AudioConfig WHISPER_AUDIO_CONFIG;

// Default audio configuration for high quality recording
extern const AudioConfig HIGH_QUALITY_AUDIO_CONFIG;

// Create a new audio recorder
// Returns NULL on failure
AudioRecorder* audio_recorder_create(const AudioConfig* config);

// Start recording to a file
// Returns 0 on success, -1 on failure
int audio_recorder_start_file(AudioRecorder* recorder, const char* filename);

// Start recording to memory buffer (for whisper processing)
// Returns 0 on success, -1 on failure
int audio_recorder_start_buffer(AudioRecorder* recorder);

// Stop recording
// Returns 0 on success, -1 on failure
int audio_recorder_stop(AudioRecorder* recorder);

// Get the recorded audio data (when recording to buffer)
// Returns pointer to audio data, size is written to out_size
// Data is valid until next start/stop call or recorder is destroyed
const float* audio_recorder_get_data(AudioRecorder* recorder, size_t* out_size);

// Get recording duration in seconds
double audio_recorder_get_duration(AudioRecorder* recorder);

// Check if currently recording
bool audio_recorder_is_recording(AudioRecorder* recorder);

// Destroy the audio recorder
void audio_recorder_destroy(AudioRecorder* recorder);

// Request microphone permissions (macOS specific)
// Returns 0 if permission granted, -1 if denied
int audio_request_permissions(void);

#endif // AUDIO_H