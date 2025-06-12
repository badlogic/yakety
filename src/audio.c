#define MINIAUDIO_IMPLEMENTATION
#include "audio.h"
#include "miniaudio.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fixed Whisper configuration (16kHz mono)
#define WHISPER_SAMPLE_RATE 16000
#define WHISPER_CHANNELS 1

// Audio recorder structure
typedef struct {
    ma_device device;
    ma_encoder encoder;

    // Buffer recording
    float *buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    size_t write_position;

    // State - accessed from both audio and main threads
    bool is_recording;      // Atomic access required
    bool is_file_recording; // Atomic access required
    char *filename;

    // Timing
    ma_uint64 start_time;
    ma_uint64 total_frames; // Written by audio thread, read by main thread
} AudioRecorder;

// Global singleton instance
static AudioRecorder *g_recorder = NULL;

// Callback for audio input
void data_callback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    AudioRecorder *recorder = (AudioRecorder *) pDevice->pUserData;

    if (!recorder || !utils_atomic_read_bool(&recorder->is_recording) || !pInput) {
        return;
    }

    const float *input = (const float *) pInput;
    size_t samples = frameCount * WHISPER_CHANNELS;

    if (utils_atomic_read_bool(&recorder->is_file_recording)) {
        // Write to file
        ma_encoder_write_pcm_frames(&recorder->encoder, pInput, frameCount, NULL);
    } else {
        // Write to buffer
        if (recorder->buffer) {
            // Resize buffer if needed
            if (recorder->write_position + samples > recorder->buffer_capacity) {
                size_t new_capacity = recorder->buffer_capacity * 2;
                if (new_capacity < recorder->write_position + samples) {
                    new_capacity = recorder->write_position + samples + 16384;
                }

                float *new_buffer = (float *) realloc(recorder->buffer, new_capacity * sizeof(float));
                if (new_buffer) {
                    recorder->buffer = new_buffer;
                    recorder->buffer_capacity = new_capacity;
                }
            }

            // Copy data to buffer
            if (recorder->write_position + samples <= recorder->buffer_capacity) {
                memcpy(recorder->buffer + recorder->write_position, input, samples * sizeof(float));
                recorder->write_position += samples;
                recorder->buffer_size = recorder->write_position;
            }
        }
    }

    recorder->total_frames += frameCount;

    (void) pOutput; // Unused
}

bool audio_recorder_init(void) {
    if (g_recorder) {
        return false; // Already initialized
    }
    g_recorder = (AudioRecorder *) calloc(1, sizeof(AudioRecorder));
    if (!g_recorder) {
        return false;
    }

    // Setup device config for Whisper (16kHz mono)
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32; // Always use float32
    deviceConfig.capture.channels = WHISPER_CHANNELS;
    deviceConfig.sampleRate = WHISPER_SAMPLE_RATE;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = g_recorder;

    // Initialize device
    if (ma_device_init(NULL, &deviceConfig, &g_recorder->device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio device\n");
        free(g_recorder);
        g_recorder = NULL;
        return false;
    }

    // Allocate initial buffer for memory recording
    g_recorder->buffer_capacity = WHISPER_SAMPLE_RATE * WHISPER_CHANNELS * 10; // 10 seconds initial
    g_recorder->buffer = (float *) malloc(g_recorder->buffer_capacity * sizeof(float));
    if (!g_recorder->buffer) {
        ma_device_uninit(&g_recorder->device);
        free(g_recorder);
        g_recorder = NULL;
        return false;
    }

    return true;
}

int audio_recorder_start_file(const char *filename) {
    if (!g_recorder || !filename || utils_atomic_read_bool(&g_recorder->is_recording)) {
        return -1;
    }

    // Setup encoder config
    ma_encoder_config encoderConfig =
        ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, WHISPER_CHANNELS, WHISPER_SAMPLE_RATE);

    // Initialize encoder
    if (ma_encoder_init_file(filename, &encoderConfig, &g_recorder->encoder) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize encoder for file: %s\n", filename);
        return -1;
    }

    // Store filename
    g_recorder->filename = utils_strdup(filename);
    utils_atomic_write_bool(&g_recorder->is_file_recording, true);
    utils_atomic_write_bool(&g_recorder->is_recording, true);
    g_recorder->start_time = 0; // We'll track frames instead of time
    g_recorder->total_frames = 0;

    // Start device
    if (ma_device_start(&g_recorder->device) != MA_SUCCESS) {
        ma_encoder_uninit(&g_recorder->encoder);
        free(g_recorder->filename);
        g_recorder->filename = NULL;
        utils_atomic_write_bool(&g_recorder->is_recording, false);
        utils_atomic_write_bool(&g_recorder->is_file_recording, false);
        return -1;
    }

    return 0;
}

int audio_recorder_start(void) {
    if (!g_recorder || utils_atomic_read_bool(&g_recorder->is_recording)) {
        return -1;
    }

    // Reset buffer
    g_recorder->write_position = 0;
    g_recorder->buffer_size = 0;
    utils_atomic_write_bool(&g_recorder->is_file_recording, false);
    utils_atomic_write_bool(&g_recorder->is_recording, true);
    g_recorder->start_time = 0; // We'll track frames instead of time
    g_recorder->total_frames = 0;

    // Start device
    if (ma_device_start(&g_recorder->device) != MA_SUCCESS) {
        utils_atomic_write_bool(&g_recorder->is_recording, false);
        return -1;
    }

    return 0;
}

int audio_recorder_stop(void) {
    if (!g_recorder || !utils_atomic_read_bool(&g_recorder->is_recording)) {
        return -1;
    }

    // Stop device
    ma_device_stop(&g_recorder->device);

    // Clean up file recording
    if (utils_atomic_read_bool(&g_recorder->is_file_recording)) {
        ma_encoder_uninit(&g_recorder->encoder);
        free(g_recorder->filename);
        g_recorder->filename = NULL;
    }

    utils_atomic_write_bool(&g_recorder->is_recording, false);
    utils_atomic_write_bool(&g_recorder->is_file_recording, false);

    return 0;
}

float *audio_recorder_get_samples(int *out_sample_count) {
    if (!g_recorder || !out_sample_count) {
        return NULL;
    }

    *out_sample_count = (int) g_recorder->buffer_size;

    // Create a copy of the buffer for the caller
    if (g_recorder->buffer_size > 0) {
        float *copy = (float *) malloc(g_recorder->buffer_size * sizeof(float));
        if (copy) {
            memcpy(copy, g_recorder->buffer, g_recorder->buffer_size * sizeof(float));
        }
        return copy;
    }

    return NULL;
}

double audio_recorder_get_duration(void) {
    if (!g_recorder) {
        return 0.0;
    }

    return (double) g_recorder->total_frames / (double) WHISPER_SAMPLE_RATE;
}

bool audio_recorder_is_recording(void) {
    return g_recorder && utils_atomic_read_bool(&g_recorder->is_recording);
}

void audio_recorder_cleanup(void) {
    if (!g_recorder) {
        return;
    }

    // Stop if recording
    if (utils_atomic_read_bool(&g_recorder->is_recording)) {
        audio_recorder_stop();
    }

    // Clean up
    ma_device_uninit(&g_recorder->device);
    free(g_recorder->buffer);
    free(g_recorder->filename);
    free(g_recorder);
    g_recorder = NULL;
}
