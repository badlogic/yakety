#ifndef DIALOG_H
#define DIALOG_H

#include <stdbool.h>
#include "keylogger.h"

void dialog_error(const char *title, const char *message);
void dialog_info(const char *title, const char *message);
bool dialog_confirm(const char *title, const char *message);
bool dialog_keycombination_capture(const char *title, const char *message, KeyCombination *result);

#ifdef __APPLE__
// macOS-specific dialogs for accessibility permission
int dialog_accessibility_permission(void);
bool dialog_wait_for_permission(void);
#endif

#endif// DIALOG_H