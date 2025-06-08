#ifndef UTILS_H
#define UTILS_H

double utils_get_time(void);
void utils_sleep_ms(int milliseconds);
const char* utils_get_model_path(void);

#ifdef __APPLE__
void utils_open_accessibility_settings(void);
#endif

#endif // UTILS_H