#ifndef DIALOG_H
#define DIALOG_H

#include <stdbool.h>

void dialog_error(const char* title, const char* message);
void dialog_info(const char* title, const char* message);
bool dialog_confirm(const char* title, const char* message);

#endif // DIALOG_H