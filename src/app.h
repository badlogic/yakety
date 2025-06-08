#ifndef APP_H
#define APP_H

#include <stdbool.h>

typedef struct {
    const char* name;
    const char* version;
    bool is_console;
} AppConfig;

int app_init(const AppConfig* config);
void app_cleanup(void);
void app_run(void);
void app_quit(void);

#endif // APP_H