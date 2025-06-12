#ifndef TRANSCRIPTION_H
#define TRANSCRIPTION_H

#ifdef __cplusplus
extern "C" {
#endif

int transcription_init(const char *model_path);
void transcription_cleanup(void);
void transcription_set_language(const char *language);
// Process audio data and return transcribed text.
// Returns malloc'd string that caller must free, or NULL on error.
// The returned string is cleaned (trimmed, filtered) and includes a trailing space
// for convenient pasting into text fields.
char *transcription_process(const float *audio_data, int n_samples, int sample_rate);

#ifdef __cplusplus
}
#endif

#endif// TRANSCRIPTION_H