#ifndef DIALOG_UTILS_H
#define DIALOG_UTILS_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>

/**
 * DPI Utilities
 */

/**
 * Get DPI scale factor for a window
 * @param hwnd Window handle
 * @return DPI scale factor (1.0 = 96 DPI)
 */
float dialog_get_dpi_scale(HWND hwnd);

/**
 * Scale a value by DPI
 * @param value Value to scale
 * @param dpi_scale DPI scale factor
 * @return Scaled value
 */
int dialog_scale_dpi(int value, float dpi_scale);

/**
 * Scale a rectangle by DPI
 * @param rect Rectangle to scale
 * @param dpi_scale DPI scale factor
 */
void dialog_scale_rect(RECT* rect, float dpi_scale);

/**
 * Theme Utilities
 */

/**
 * Check if Windows is in dark mode
 * @return True if dark mode is enabled
 */
bool dialog_is_dark_mode(void);

/**
 * Apply dark mode to window if supported
 * @param hwnd Window handle
 * @param enable True to enable dark mode
 * @return True if applied successfully
 */
bool dialog_apply_dark_mode(HWND hwnd, bool enable);

/**
 * String Utilities
 */

/**
 * Convert UTF-8 string to wide string
 * @param utf8 UTF-8 string
 * @return Wide string (must be freed with free())
 */
wchar_t* dialog_utf8_to_wide(const char* utf8);

/**
 * Convert wide string to UTF-8
 * @param wide Wide string
 * @return UTF-8 string (must be freed with free())
 */
char* dialog_wide_to_utf8(const wchar_t* wide);

/**
 * Drawing Utilities
 */

/**
 * Draw rounded rectangle
 * @param hdc Device context
 * @param rect Rectangle bounds
 * @param radius Corner radius
 * @param pen Pen for border (can be NULL)
 * @param brush Brush for fill (can be NULL)
 */
void dialog_draw_rounded_rect(HDC hdc, const RECT* rect, int radius, HPEN pen, HBRUSH brush);

/**
 * Draw text with shadow effect
 * @param hdc Device context
 * @param text Text to draw
 * @param rect Text rectangle
 * @param format Text format flags
 * @param text_color Main text color
 * @param shadow_color Shadow color
 * @param font Font to use
 */
void dialog_draw_text_with_shadow(HDC hdc, const wchar_t* text, const RECT* rect, 
                                 UINT format, COLORREF text_color, COLORREF shadow_color, 
                                 HFONT font);

/**
 * Window Utilities
 */

/**
 * Center window on parent or screen
 * @param hwnd Window to center
 * @param parent Parent window (NULL for screen)
 */
void dialog_center_window(HWND hwnd, HWND parent);

/**
 * Get monitor containing window
 * @param hwnd Window handle
 * @return Monitor handle
 */
HMONITOR dialog_get_monitor(HWND hwnd);

/**
 * Get work area for monitor
 * @param monitor Monitor handle
 * @param work_area Output work area rectangle
 * @return True on success
 */
bool dialog_get_work_area(HMONITOR monitor, RECT* work_area);

/**
 * Animation Utilities
 */

/**
 * Animate window fade in
 * @param hwnd Window handle
 * @param duration Duration in milliseconds
 */
void dialog_animate_fade_in(HWND hwnd, DWORD duration);

/**
 * Animate window fade out
 * @param hwnd Window handle
 * @param duration Duration in milliseconds
 */
void dialog_animate_fade_out(HWND hwnd, DWORD duration);

/**
 * Color Utilities
 */

/**
 * Blend two colors
 * @param color1 First color
 * @param color2 Second color
 * @param alpha Blend factor (0.0 to 1.0)
 * @return Blended color
 */
COLORREF dialog_blend_colors(COLORREF color1, COLORREF color2, float alpha);

/**
 * Lighten a color
 * @param color Original color
 * @param amount Amount to lighten (0.0 to 1.0)
 * @return Lightened color
 */
COLORREF dialog_lighten_color(COLORREF color, float amount);

/**
 * Darken a color
 * @param color Original color
 * @param amount Amount to darken (0.0 to 1.0)
 * @return Darkened color
 */
COLORREF dialog_darken_color(COLORREF color, float amount);

/**
 * Control Utilities
 */

/**
 * Create a properly themed button
 * @param parent Parent window
 * @param text Button text
 * @param x X position
 * @param y Y position
 * @param width Button width
 * @param height Button height
 * @param id Control ID
 * @param is_default True if default button
 * @return Button handle
 */
HWND dialog_create_button(HWND parent, const char* text, int x, int y, 
                         int width, int height, int id, bool is_default);

/**
 * Create a label control
 * @param parent Parent window
 * @param text Label text
 * @param x X position
 * @param y Y position
 * @param width Label width
 * @param height Label height
 * @param id Control ID
 * @return Label handle
 */
HWND dialog_create_label(HWND parent, const char* text, int x, int y, 
                        int width, int height, int id);

#endif // DIALOG_UTILS_H