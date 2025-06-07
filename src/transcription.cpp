extern "C" {
#include "transcription.h"
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <fstream>
#include "../whisper.cpp/include/whisper.h"

static struct whisper_context* ctx = NULL;

int transcription_init(const char* model_path) {
    if (ctx != NULL) {
        whisper_free(ctx);
        ctx = NULL;
    }
    
    printf("🧠 Loading Whisper model: %s\n", model_path);
    
    // Initialize whisper context with default params
    struct whisper_context_params params = whisper_context_default_params();
    ctx = whisper_init_from_file_with_params(model_path, params);
    if (ctx == NULL) {
        fprintf(stderr, "ERROR: Failed to initialize whisper context from %s\n", model_path);
        return -1;
    }
    
    printf("✅ Whisper model loaded successfully\n");
    return 0;
}

int transcribe_audio(const float* audio_data, int n_samples, char* result, size_t result_size) {
    if (ctx == NULL) {
        fprintf(stderr, "ERROR: Whisper not initialized\n");
        return -1;
    }
    
    if (audio_data == NULL || result == NULL || n_samples <= 0) {
        fprintf(stderr, "ERROR: Invalid parameters for transcription\n");
        return -1;
    }
    
    printf("🧠 Transcribing %d audio samples...\n", n_samples);
    
    // Set up whisper parameters
    struct whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = false;
    wparams.print_special = false;
    wparams.translate = false;
    wparams.language = "en";
    wparams.n_threads = 4;
    wparams.offset_ms = 0;
    wparams.duration_ms = 0;
    
    // Run transcription
    if (whisper_full(ctx, wparams, audio_data, n_samples) != 0) {
        fprintf(stderr, "ERROR: Failed to run whisper transcription\n");
        return -1;
    }
    
    // Get transcription result
    const int n_segments = whisper_full_n_segments(ctx);
    if (n_segments == 0) {
        printf("⚠️  No speech detected\n");
        strncpy(result, "", result_size);
        return 0;
    }
    
    // Concatenate all segments
    result[0] = '\0';
    for (int i = 0; i < n_segments; ++i) {
        const char* text = whisper_full_get_segment_text(ctx, i);
        if (text) {
            size_t current_len = strlen(result);
            size_t text_len = strlen(text);
            
            // Check if we have space for the text plus space separator plus null terminator
            if (current_len + text_len + 2 < result_size) {
                if (current_len > 0) {
                    strcat(result, " ");
                }
                strcat(result, text);
            } else {
                fprintf(stderr, "WARNING: Transcription result truncated\n");
                break;
            }
        }
    }
    
    printf("✅ Transcription complete: \"%s\"\n", result);
    return 0;
}

int transcribe_file(const char* audio_file, char* result, size_t result_size) {
    if (ctx == NULL) {
        fprintf(stderr, "ERROR: Whisper not initialized\n");
        return -1;
    }
    
    printf("🎵 Loading audio file: %s\n", audio_file);
    
    // Proper WAV parser
    std::vector<float> pcmf32;
    
    std::ifstream file(audio_file, std::ios::binary);
    if (!file.is_open()) {
        fprintf(stderr, "ERROR: Could not open audio file: %s\n", audio_file);
        return -1;
    }
    
    // Read RIFF header
    char riff[4];
    uint32_t file_size;
    char wave[4];
    file.read(riff, 4);
    file.read(reinterpret_cast<char*>(&file_size), 4);
    file.read(wave, 4);
    
    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0) {
        fprintf(stderr, "ERROR: Not a valid WAV file\n");
        file.close();
        return -1;
    }
    
    // Find fmt chunk
    uint16_t format_tag = 0;
    uint16_t channels = 0;
    uint32_t sample_rate = 0;
    uint16_t bits_per_sample = 0;
    uint32_t data_size = 0;
    bool fmt_found = false;
    bool data_found = false;
    
    while (!fmt_found || !data_found) {
        char chunk_id[4];
        uint32_t chunk_size;
        
        if (!file.read(chunk_id, 4) || !file.read(reinterpret_cast<char*>(&chunk_size), 4)) {
            fprintf(stderr, "ERROR: Unexpected end of WAV file\n");
            file.close();
            return -1;
        }
        
        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&format_tag), 2);
            file.read(reinterpret_cast<char*>(&channels), 2);
            file.read(reinterpret_cast<char*>(&sample_rate), 4);
            file.seekg(6, std::ios::cur); // skip byte_rate and block_align
            file.read(reinterpret_cast<char*>(&bits_per_sample), 2);
            file.seekg(chunk_size - 16, std::ios::cur); // skip any extra fmt data
            fmt_found = true;
        } else if (strncmp(chunk_id, "data", 4) == 0) {
            data_size = chunk_size;
            data_found = true;
            
            // If data_size is 0, calculate actual size from file
            if (data_size == 0) {
                std::streampos current_pos = file.tellg();
                file.seekg(0, std::ios::end);
                std::streampos end_pos = file.tellg();
                data_size = (uint32_t)(end_pos - current_pos);
                file.seekg(current_pos); // return to data start
                printf("🎵 Data chunk size was 0, calculated actual size: %u bytes\n", data_size);
            }
            
            break; // data chunk found, ready to read audio
        } else {
            // Skip unknown chunk
            file.seekg(chunk_size, std::ios::cur);
        }
    }
    
    if (!fmt_found || !data_found) {
        fprintf(stderr, "ERROR: Missing fmt or data chunk in WAV file\n");
        file.close();
        return -1;
    }
    
    printf("🎵 WAV: %s, %dHz, %d channels, %d bits, format %d\n", 
           audio_file, sample_rate, channels, bits_per_sample, format_tag);
    
    // Read audio data based on format
    uint32_t num_samples = data_size / (bits_per_sample / 8) / channels;
    pcmf32.reserve(num_samples);
    
    if (format_tag == 3 && bits_per_sample == 32) {
        // 32-bit float PCM
        for (uint32_t i = 0; i < num_samples; i++) {
            float sample_sum = 0.0f;
            
            // Read all channels and average them for mono output
            for (uint16_t ch = 0; ch < channels; ch++) {
                float sample;
                if (!file.read(reinterpret_cast<char*>(&sample), 4)) {
                    break;
                }
                sample_sum += sample;
            }
            
            if (file.fail()) break;
            pcmf32.push_back(sample_sum / channels); // Average channels for mono
        }
    } else if (format_tag == 1 && bits_per_sample == 16) {
        // 16-bit integer PCM
        for (uint32_t i = 0; i < num_samples; i++) {
            float sample_sum = 0.0f;
            
            // Read all channels and average them for mono output
            for (uint16_t ch = 0; ch < channels; ch++) {
                int16_t sample;
                if (!file.read(reinterpret_cast<char*>(&sample), 2)) {
                    break;
                }
                sample_sum += static_cast<float>(sample) / 32768.0f;
            }
            
            if (file.fail()) break;
            pcmf32.push_back(sample_sum / channels); // Average channels for mono
        }
    } else {
        fprintf(stderr, "ERROR: Unsupported WAV format (format=%d, bits=%d)\n", format_tag, bits_per_sample);
        file.close();
        return -1;
    }
    
    file.close();
    
    if (pcmf32.empty()) {
        fprintf(stderr, "ERROR: No audio data found in file: %s\n", audio_file);
        return -1;
    }
    
    printf("🎵 Loaded %zu audio samples\n", pcmf32.size());
    
    // Convert to C-style array access for transcription
    const float* audio_data = pcmf32.data();
    int n_samples = (int)pcmf32.size();
    
    return transcribe_audio(audio_data, n_samples, result, result_size);
}

void transcription_cleanup(void) {
    if (ctx != NULL) {
        printf("🧹 Cleaning up Whisper context\n");
        whisper_free(ctx);
        ctx = NULL;
    }
}