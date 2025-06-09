#ifndef TRANSCRIPTION_H
#define TRANSCRIPTION_H

#ifdef __cplusplus
extern "C" {
#endif

int transcription_init(const char* model_path);
void transcription_cleanup(void);
void transcription_set_language(const char* language);
char* transcription_process(const float* audio_data, int n_samples, int sample_rate);

#ifdef __cplusplus
}
#endif

#endif // TRANSCRIPTION_H