#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stddef.h>

// Audio recorder singleton API
// Initialize the audio recorder (fixed to Whisper config: 16kHz mono)
// Returns true on success, false on failure
bool audio_recorder_init(void);

// Cleanup the audio recorder
void audio_recorder_cleanup(void);

// Start recording to memory buffer
// Returns 0 on success, -1 on failure
int audio_recorder_start(void);

// Start recording to file
// Returns 0 on success, -1 on failure
int audio_recorder_start_file(const char *filename);

// Stop recording
// Returns 0 on success, -1 on failure
int audio_recorder_stop(void);

// Get the recorded audio samples
// Returns pointer to audio data, count is written to out_sample_count
// Caller must free the returned buffer
float *audio_recorder_get_samples(int *out_sample_count);

// Get recording duration in seconds
double audio_recorder_get_duration(void);

// Check if currently recording
bool audio_recorder_is_recording(void);

#endif // AUDIO_H