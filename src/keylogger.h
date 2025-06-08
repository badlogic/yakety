#ifndef KEYLOGGER_H
#define KEYLOGGER_H

typedef void (*KeyCallback)(void* userdata);

int keylogger_init(KeyCallback on_press, KeyCallback on_release, void* userdata);
void keylogger_cleanup(void);

#endif // KEYLOGGER_H