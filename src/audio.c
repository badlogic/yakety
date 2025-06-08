#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "audio.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Default configurations
const AudioConfig WHISPER_AUDIO_CONFIG = {
    .sample_rate = 16000,
    .channels = 1,
    .bits_per_sample = 32  // miniaudio uses float32 internally
};

const AudioConfig HIGH_QUALITY_AUDIO_CONFIG = {
    .sample_rate = 44100,
    .channels = 2,
    .bits_per_sample = 32
};

// Audio recorder structure
struct AudioRecorder {
    ma_device device;
    ma_encoder encoder;
    AudioConfig config;
    
    // Buffer recording
    float* buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    size_t write_position;
    
    // State
    bool is_recording;
    bool is_file_recording;
    char* filename;
    
    // Timing
    ma_uint64 start_time;
    ma_uint64 total_frames;
};

// Callback for audio input
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    AudioRecorder* recorder = (AudioRecorder*)pDevice->pUserData;
    
    if (!recorder || !recorder->is_recording || !pInput) {
        return;
    }
    
    const float* input = (const float*)pInput;
    size_t samples = frameCount * recorder->config.channels;
    
    if (recorder->is_file_recording) {
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
                
                float* new_buffer = (float*)realloc(recorder->buffer, new_capacity * sizeof(float));
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
    
    (void)pOutput; // Unused
}

AudioRecorder* audio_recorder_create(const AudioConfig* config) {
    AudioRecorder* recorder = (AudioRecorder*)calloc(1, sizeof(AudioRecorder));
    if (!recorder) {
        return NULL;
    }
    
    // Copy config
    if (config) {
        recorder->config = *config;
    } else {
        recorder->config = WHISPER_AUDIO_CONFIG;
    }
    
    // Setup device config
    ma_device_config deviceConfig = ma_device_config_init(ma_device_type_capture);
    deviceConfig.capture.format = ma_format_f32;  // Always use float32
    deviceConfig.capture.channels = recorder->config.channels;
    deviceConfig.sampleRate = recorder->config.sample_rate;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = recorder;
    
    // Initialize device
    if (ma_device_init(NULL, &deviceConfig, &recorder->device) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize audio device\n");
        free(recorder);
        return NULL;
    }
    
    // Allocate initial buffer for memory recording
    recorder->buffer_capacity = recorder->config.sample_rate * recorder->config.channels * 10; // 10 seconds initial
    recorder->buffer = (float*)malloc(recorder->buffer_capacity * sizeof(float));
    if (!recorder->buffer) {
        ma_device_uninit(&recorder->device);
        free(recorder);
        return NULL;
    }
    
    return recorder;
}

int audio_recorder_start_file(AudioRecorder* recorder, const char* filename) {
    if (!recorder || !filename || recorder->is_recording) {
        return -1;
    }
    
    // Setup encoder config
    ma_encoder_config encoderConfig = ma_encoder_config_init(
        ma_encoding_format_wav,
        ma_format_f32,
        recorder->config.channels,
        recorder->config.sample_rate
    );
    
    // Initialize encoder
    if (ma_encoder_init_file(filename, &encoderConfig, &recorder->encoder) != MA_SUCCESS) {
        fprintf(stderr, "Failed to initialize encoder for file: %s\n", filename);
        return -1;
    }
    
    // Store filename
    recorder->filename = utils_strdup(filename);
    recorder->is_file_recording = true;
    recorder->is_recording = true;
    recorder->start_time = 0;  // We'll track frames instead of time
    recorder->total_frames = 0;
    
    // Start device
    if (ma_device_start(&recorder->device) != MA_SUCCESS) {
        ma_encoder_uninit(&recorder->encoder);
        free(recorder->filename);
        recorder->filename = NULL;
        recorder->is_recording = false;
        recorder->is_file_recording = false;
        return -1;
    }
    
    return 0;
}

int audio_recorder_start(AudioRecorder* recorder) {
    if (!recorder || recorder->is_recording) {
        return -1;
    }
    
    // Reset buffer
    recorder->write_position = 0;
    recorder->buffer_size = 0;
    recorder->is_file_recording = false;
    recorder->is_recording = true;
    recorder->start_time = 0;  // We'll track frames instead of time
    recorder->total_frames = 0;
    
    // Start device
    if (ma_device_start(&recorder->device) != MA_SUCCESS) {
        recorder->is_recording = false;
        return -1;
    }
    
    return 0;
}

int audio_recorder_stop(AudioRecorder* recorder) {
    if (!recorder || !recorder->is_recording) {
        return -1;
    }
    
    // Stop device
    ma_device_stop(&recorder->device);
    
    // Clean up file recording
    if (recorder->is_file_recording) {
        ma_encoder_uninit(&recorder->encoder);
        free(recorder->filename);
        recorder->filename = NULL;
    }
    
    recorder->is_recording = false;
    recorder->is_file_recording = false;
    
    return 0;
}

float* audio_recorder_get_samples(AudioRecorder* recorder, int* out_sample_count) {
    if (!recorder || !out_sample_count) {
        return NULL;
    }
    
    *out_sample_count = (int)recorder->buffer_size;
    
    // Create a copy of the buffer for the caller
    if (recorder->buffer_size > 0) {
        float* copy = (float*)malloc(recorder->buffer_size * sizeof(float));
        if (copy) {
            memcpy(copy, recorder->buffer, recorder->buffer_size * sizeof(float));
        }
        return copy;
    }
    
    return NULL;
}

double audio_recorder_get_duration(AudioRecorder* recorder) {
    if (!recorder || recorder->config.sample_rate == 0) {
        return 0.0;
    }
    
    return (double)recorder->total_frames / (double)recorder->config.sample_rate;
}

bool audio_recorder_is_recording(AudioRecorder* recorder) {
    return recorder && recorder->is_recording;
}

void audio_recorder_destroy(AudioRecorder* recorder) {
    if (!recorder) {
        return;
    }
    
    // Stop if recording
    if (recorder->is_recording) {
        audio_recorder_stop(recorder);
    }
    
    // Clean up
    ma_device_uninit(&recorder->device);
    free(recorder->buffer);
    free(recorder->filename);
    free(recorder);
}

