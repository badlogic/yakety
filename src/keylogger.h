#ifndef KEYLOGGER_H
#define KEYLOGGER_H

#include <stdint.h>

typedef void (*KeyCallback)(void* userdata);

// Key combination structure using raw key codes and modifier flags
typedef struct {
    uint16_t keycode;        // Main key code (0 if modifier-only)
    uint32_t modifier_flags; // Modifier flags
} KeyCombination;

int keylogger_init(KeyCallback on_press, KeyCallback on_release, void* userdata);
void keylogger_cleanup(void);

// Pause/resume keylogger reporting
void keylogger_pause(void);
void keylogger_resume(void);

// Set custom key combination to monitor (instead of just FN key)
void keylogger_set_combination(const KeyCombination* combo);

// Get the default FN key combination
KeyCombination keylogger_get_fn_combination(void);

#endif // KEYLOGGER_H