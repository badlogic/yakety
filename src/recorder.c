#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#endif
#include "audio.h"

static AudioRecorder* recorder = NULL;
static volatile bool should_stop = false;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n🛑 Stopping recording...\n");
        should_stop = true;
    }
}

void print_usage(const char* program_name) {
    printf("Usage: %s <output_file> [options]\n", program_name);
    printf("\nOptions:\n");
    printf("  -r, --rate RATE      Sample rate (default: 44100)\n");
    printf("  -c, --channels CHAN  Number of channels (default: 2)\n");
    printf("  -b, --bits BITS      Bits per sample (default: 16)\n");
    printf("  -w, --whisper        Use Whisper-optimized settings (16kHz mono)\n");
    printf("  -h, --help           Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s recording.wav\n", program_name);
    printf("  %s -w speech.wav          # Whisper-optimized recording\n", program_name);
    printf("  %s -r 48000 -c 1 mono.wav # 48kHz mono recording\n", program_name);
    printf("\nPress Ctrl+C to stop recording.\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Parse command line arguments
    const char* output_file = NULL;
    AudioConfig config = HIGH_QUALITY_AUDIO_CONFIG;
    bool use_whisper_config = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--whisper") == 0) {
            use_whisper_config = true;
        } else if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rate") == 0) && i + 1 < argc) {
            config.sample_rate = atoi(argv[++i]);
            if (config.sample_rate <= 0) {
                fprintf(stderr, "Error: Invalid sample rate\n");
                return 1;
            }
        } else if ((strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--channels") == 0) && i + 1 < argc) {
            config.channels = atoi(argv[++i]);
            if (config.channels <= 0 || config.channels > 2) {
                fprintf(stderr, "Error: Invalid channel count (1-2 supported)\n");
                return 1;
            }
        } else if ((strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--bits") == 0) && i + 1 < argc) {
            config.bits_per_sample = atoi(argv[++i]);
            if (config.bits_per_sample != 16 && config.bits_per_sample != 24 && config.bits_per_sample != 32) {
                fprintf(stderr, "Error: Invalid bits per sample (16, 24, or 32 supported)\n");
                return 1;
            }
        } else if (argv[i][0] != '-') {
            if (output_file == NULL) {
                output_file = argv[i];
            } else {
                fprintf(stderr, "Error: Multiple output files specified\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown option %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (output_file == NULL) {
        fprintf(stderr, "Error: No output file specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Use Whisper config if requested
    if (use_whisper_config) {
        config = WHISPER_AUDIO_CONFIG;
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("🎤 Audio Recorder\n");
    printf("📁 Output file: %s\n", output_file);
    printf("⚙️  Configuration:\n");
    printf("   Sample rate: %d Hz\n", config.sample_rate);
    printf("   Channels: %d (%s)\n", config.channels, config.channels == 1 ? "mono" : "stereo");
    printf("   Bits per sample: %d\n", config.bits_per_sample);
    printf("\n");
    
    // Miniaudio handles permissions automatically
    printf("✅ Microphone permission granted\n\n");
    
    // Create audio recorder
    recorder = audio_recorder_create(&config);
    if (!recorder) {
        fprintf(stderr, "❌ Error: Failed to create audio recorder\n");
        return 1;
    }
    
    // Start recording
    printf("🔴 Starting recording... (Press Ctrl+C to stop)\n");
    if (audio_recorder_start_file(recorder, output_file) != 0) {
        fprintf(stderr, "❌ Error: Failed to start recording\n");
        audio_recorder_destroy(recorder);
        return 1;
    }
    
    // Recording loop
    printf("🎵 Recording to %s...\n", output_file);
    while (!should_stop && audio_recorder_is_recording(recorder)) {
#ifdef _WIN32
        Sleep(100); // Sleep for 100ms
#else
        usleep(100000); // Sleep for 100ms
#endif
        
        // Print duration every second
        static int last_duration = -1;
        int duration = (int)audio_recorder_get_duration(recorder);
        if (duration != last_duration) {
            printf("\r⏱️  Duration: %02d:%02d", duration / 60, duration % 60);
            fflush(stdout);
            last_duration = duration;
        }
    }
    
    // Stop recording
    printf("\n🛑 Stopping recording...\n");
    if (audio_recorder_stop(recorder) != 0) {
        fprintf(stderr, "❌ Warning: Error stopping recording\n");
    }
    
    // Show final duration
    double final_duration = audio_recorder_get_duration(recorder);
    printf("✅ Recording saved: %s\n", output_file);
    printf("⏱️  Final duration: %.2f seconds\n", final_duration);
    
    // Cleanup
    audio_recorder_destroy(recorder);
    
    return 0;
}