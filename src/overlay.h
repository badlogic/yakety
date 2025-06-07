#ifndef OVERLAY_H
#define OVERLAY_H

// Initialize the overlay window system
void overlay_init(void);

// Show overlay with given message
void overlay_show(const char* message);

// Hide the overlay
void overlay_hide(void);

// Show overlay with transcription result
void overlay_show_result(const char* text);

// Cleanup overlay resources
void overlay_cleanup(void);

#endif // OVERLAY_H