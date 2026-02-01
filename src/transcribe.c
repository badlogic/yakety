#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "transcription.h"
#include "utils.h"

// Simple WAV file reader
typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t data_size;
    float *samples;
    int sample_count;
} WavFile;

static int read_wav_file(const char *filename, WavFile *wav) {
    FILE *file = utils_fopen_read_binary(filename);
    if (!file) {
        log_error("ERROR: Could not open file: %s\n", filename);
        return -1;
    }

    // Read RIFF header
    char riff[4];
    uint32_t file_size;
    char wave[4];

    if (fread(riff, 1, 4, file) != 4 ||
        fread(&file_size, 4, 1, file) != 1 ||
        fread(wave, 1, 4, file) != 4) {
        log_error("ERROR: Failed to read WAV header\n");
        fclose(file);
        return -1;
    }

    if (strncmp(riff, "RIFF", 4) != 0 || strncmp(wave, "WAVE", 4) != 0) {
        log_error("ERROR: Not a valid WAV file\n");
        fclose(file);
        return -1;
    }

    // Read chunks
    uint16_t format_tag = 0;
    wav->channels = 0;
    wav->sample_rate = 0;
    wav->bits_per_sample = 0;
    wav->data_size = 0;

    while (!feof(file)) {
        char chunk_id[4];
        uint32_t chunk_size;

        if (fread(chunk_id, 1, 4, file) != 4)
            break;
        if (fread(&chunk_size, 4, 1, file) != 1)
            break;

        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            if (fread(&format_tag, 2, 1, file) != 1 ||
                fread(&wav->channels, 2, 1, file) != 1 ||
                fread(&wav->sample_rate, 4, 1, file) != 1) {
                log_error("ERROR: Failed to read fmt chunk\n");
                fclose(file);
                return -1;
            }
            fseek(file, 6, SEEK_CUR); // skip byte_rate and block_align
            if (fread(&wav->bits_per_sample, 2, 1, file) != 1) {
                log_error("ERROR: Failed to read bits_per_sample\n");
                fclose(file);
                return -1;
            }
            fseek(file, chunk_size - 16, SEEK_CUR); // skip rest of fmt chunk
        } else if (strncmp(chunk_id, "data", 4) == 0) {
            wav->data_size = chunk_size;

            // Allocate buffer for samples
            int bytes_per_sample = wav->bits_per_sample / 8;
            wav->sample_count = wav->data_size / bytes_per_sample / wav->channels;
            wav->samples = (float *) malloc(wav->sample_count * sizeof(float));

            if (!wav->samples) {
                log_error("ERROR: Failed to allocate memory for samples\n");
                fclose(file);
                return -1;
            }

            // Read and convert samples to float
            if (format_tag == 3 && wav->bits_per_sample == 32) {
                // 32-bit float
                for (int i = 0; i < wav->sample_count; i++) {
                    float sample_sum = 0.0f;
                    for (int ch = 0; ch < wav->channels; ch++) {
                        float sample;
                        if (fread(&sample, 4, 1, file) != 1) {
                            log_error("ERROR: Failed to read sample data\n");
                            free(wav->samples);
                            fclose(file);
                            return -1;
                        }
                        sample_sum += sample;
                    }
                    wav->samples[i] = sample_sum / wav->channels;
                }
            } else if (format_tag == 1 && wav->bits_per_sample == 16) {
                // 16-bit PCM
                for (int i = 0; i < wav->sample_count; i++) {
                    float sample_sum = 0.0f;
                    for (int ch = 0; ch < wav->channels; ch++) {
                        int16_t sample;
                        if (fread(&sample, 2, 1, file) != 1) {
                            log_error("ERROR: Failed to read sample data\n");
                            free(wav->samples);
                            fclose(file);
                            return -1;
                        }
                        sample_sum += (float) sample / 32768.0f;
                    }
                    wav->samples[i] = sample_sum / wav->channels;
                }
            } else {
                log_error("ERROR: Unsupported WAV format (format=%d, bits=%d)\n", format_tag, wav->bits_per_sample);
                free(wav->samples);
                fclose(file);
                return -1;
            }

            break; // Done reading
        } else {
            // Skip unknown chunk
            fseek(file, chunk_size, SEEK_CUR);
        }
    }

    fclose(file);

    if (wav->sample_count == 0) {
        log_error("ERROR: No audio data found in file\n");
        return -1;
    }

    log_info("📊 WAV file info: %d Hz, %d channels, %d bits, %d samples\n", wav->sample_rate, wav->channels,
             wav->bits_per_sample, wav->sample_count);

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <audio_file.wav> [model_path]\n", argv[0]);
        printf("Example: %s ./out.wav\n", argv[0]);
        printf("Example: %s ./out.wav /path/to/ggml-model.bin\n", argv[0]);
        return 1;
    }

    const char *audio_file = argv[1];
    const char *model_path = NULL;

    // Check if model path was provided as second argument
    if (argc >= 3) {
        model_path = argv[2];
    } else {
        // Get default model path
        model_path = utils_get_model_path();
        if (!model_path) {
            printf("Error: Could not find Whisper model file\n");
            return 1;
        }
    }

    printf("=== Whisper Transcription Performance Test ===\n");
    printf("Audio file: %s\n", audio_file);

    double start_time = utils_now();

    printf("Using model: %s\n", model_path);
    printf("Loading model...\n");
    double model_load_start = utils_now();

    if (transcription_init(model_path) != 0) {
        printf("Error: Failed to initialize transcription\n");
        return 1;
    }

    transcription_set_language("auto");

    double model_load_time = utils_now() - model_load_start;
    printf("Model loaded in %.2f ms\n", model_load_time * 1000.0);

    // Read WAV file
    WavFile wav = {0};
    if (read_wav_file(audio_file, &wav) != 0) {
        printf("Error: Failed to read WAV file\n");
        transcription_cleanup();
        return 1;
    }

    // Calculate audio duration
    double audio_duration_sec = (double) wav.sample_count / wav.sample_rate;
    printf("Audio duration: %.2f seconds (%d samples at %d Hz)\n", audio_duration_sec, wav.sample_count,
           wav.sample_rate);

    // Transcribe audio
    printf("Starting transcription...\n");
    double transcribe_start = utils_now();
    char *result = transcription_process(wav.samples, wav.sample_count, wav.sample_rate);
    double transcribe_time = utils_now() - transcribe_start;

    if (result) {
        printf("\n=== RESULTS ===\n");
        printf("Transcription: \"%s\"\n", result);
        printf("Transcription time: %.2f ms (%.3f seconds)\n", transcribe_time * 1000.0, transcribe_time);

        // Calculate real-time factor
        double rtf = transcribe_time / audio_duration_sec;
        printf("Real-time factor: %.2fx %s\n", rtf, rtf < 1.0 ? "(FASTER than real-time)" : "(SLOWER than real-time)");

        if (rtf < 1.0) {
            printf("Performance: EXCELLENT - Can transcribe in real-time!\n");
        } else if (rtf < 2.0) {
            printf("Performance: GOOD - Close to real-time\n");
        } else {
            printf("Performance: SLOW - Much slower than real-time\n");
        }

        free(result);
    } else {
        printf("Error: Transcription failed\n");
    }

    double total_time = utils_now() - start_time;
    printf("Total time: %.2f ms\n", total_time * 1000.0);

    // Cleanup
    free(wav.samples);
    transcription_cleanup();

    return 0;
}