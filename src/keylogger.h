#ifndef KEYLOGGER_H
#define KEYLOGGER_H

#include <stdint.h>

typedef void (*KeyCallback)(void *userdata);

// Unified key info structure for both platforms
typedef struct {
	uint32_t code; // keycode on macOS, scancode on Windows
	uint32_t flags;// modifiers on macOS, extended flag (0/1) on Windows
} KeyInfo;

// Key combination structure supporting multiple keys
typedef struct {
	KeyInfo keys[4];// macOS uses only keys[0], Windows can use multiple
	int count;      // Number of keys in combination
} KeyCombination;

int keylogger_init(KeyCallback on_press, KeyCallback on_release, void *userdata);
void keylogger_cleanup(void);

// Pause/resume keylogger reporting
void keylogger_pause(void);
void keylogger_resume(void);

// Set custom key combination to monitor (instead of just FN key)
void keylogger_set_combination(const KeyCombination *combo);

// Get the default FN key combination
KeyCombination keylogger_get_fn_combination(void);

#endif// KEYLOGGER_H