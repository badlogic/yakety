#ifndef TRANSCRIPTION_H
#define TRANSCRIPTION_H

#include <stdbool.h>
#include <stddef.h>

// Initialize the whisper model
// Returns 0 on success, -1 on failure
int transcription_init(const char* model_path);

// Transcribe audio data to text
// audio_data: float array of audio samples (16kHz mono)
// n_samples: number of samples in audio_data
// result: output buffer for transcribed text (caller must allocate)
// result_size: size of result buffer
// Returns 0 on success, -1 on failure
int transcribe_audio(const float* audio_data, int n_samples, char* result, size_t result_size);

// Transcribe audio file to text (for testing only)
// audio_file: path to WAV file (16kHz mono recommended)
// result: output buffer for transcribed text (caller must allocate)
// result_size: size of result buffer
// Returns 0 on success, -1 on failure
int transcribe_file(const char* audio_file, char* result, size_t result_size);

// Cleanup resources
void transcription_cleanup(void);

#endif // TRANSCRIPTION_H