#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#define sleep(x) Sleep((x) * 1000)
#endif
#include "audio.h"

// Using singleton audio recorder
static volatile bool should_stop = false;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\n🛑 Stopping recording...\n");
        should_stop = true;
    }
}

void print_usage(const char *program_name) {
    printf("Usage: %s [options] <output_file.wav>\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help           Show this help message\n");
    printf("\nRecords audio in 16kHz mono format (Whisper compatible).\n");
    printf("\nExample:\n");
    printf("  %s recording.wav     # Record to file\n", program_name);
    printf("\nPress Ctrl+C to stop recording.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Parse command line arguments
    const char *output_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
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

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("🎤 Audio Recorder\n");
    printf("📁 Output file: %s\n", output_file);
    printf("⚙️  Configuration: 16kHz mono (Whisper compatible)\n");
    printf("\n");

    // Miniaudio handles permissions automatically
    printf("✅ Microphone permission granted\n\n");

    // Initialize audio recorder
    if (!audio_recorder_init()) {
        fprintf(stderr, "❌ Error: Failed to initialize audio recorder\n");
        return 1;
    }

    // Start recording
    printf("🔴 Starting recording... (Press Ctrl+C to stop)\n");
    if (audio_recorder_start_file(output_file) != 0) {
        fprintf(stderr, "❌ Error: Failed to start recording\n");
        audio_recorder_cleanup();
        return 1;
    }

    // Recording loop
    printf("🎵 Recording to %s...\n", output_file);
    while (!should_stop && audio_recorder_is_recording()) {
#ifdef _WIN32
        Sleep(100); // Sleep for 100ms
#else
        usleep(100000); // Sleep for 100ms
#endif

        // Print duration every second
        static int last_duration = -1;
        int duration = (int) audio_recorder_get_duration();
        if (duration != last_duration) {
            printf("\r⏱️  Duration: %02d:%02d", duration / 60, duration % 60);
            fflush(stdout);
            last_duration = duration;
        }
    }

    // Stop recording
    printf("\n🛑 Stopping recording...\n");
    if (audio_recorder_stop() != 0) {
        fprintf(stderr, "❌ Warning: Error stopping recording\n");
    }

    // Show final duration
    double final_duration = audio_recorder_get_duration();
    printf("✅ Recording saved: %s\n", output_file);
    printf("⏱️  Final duration: %.2f seconds\n", final_duration);

    // Cleanup
    audio_recorder_cleanup();

    return 0;
}