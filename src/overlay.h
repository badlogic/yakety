#ifndef OVERLAY_H
#define OVERLAY_H

// Initialize the overlay window system
void overlay_init(void);

// Show overlay with given message
void overlay_show(const char* message);

// Hide the overlay
void overlay_hide(void);

// Cleanup overlay resources
void overlay_cleanup(void);

#endif // OVERLAY_H