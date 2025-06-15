#ifndef DIALOG_FRAMEWORK_H
#define DIALOG_FRAMEWORK_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>

// Design constants matching macOS SwiftUI dialogs
#define DIALOG_PADDING 20
#define DIALOG_HEADER_HEIGHT 76
#define DIALOG_BUTTON_HEIGHT 32
#define DIALOG_BUTTON_SPACING 12
#define DIALOG_TITLE_BAR_HEIGHT 32
#define APP_ICON_SIZE 32

// Font constants
#define DIALOG_FONT_FAMILY L"Segoe UI"
#define DIALOG_TITLE_FONT_SIZE 18
#define DIALOG_BODY_FONT_SIZE 14
#define DIALOG_BUTTON_FONT_SIZE 14

// Component IDs
#define DIALOG_ID_BASE 2000
#define DIALOG_ID_HEADER (DIALOG_ID_BASE + 1)
#define DIALOG_ID_CONTENT (DIALOG_ID_BASE + 2)
#define DIALOG_ID_BUTTON_AREA (DIALOG_ID_BASE + 3)
#define DIALOG_ID_CANCEL_BUTTON (DIALOG_ID_BASE + 10)
#define DIALOG_ID_OK_BUTTON (DIALOG_ID_BASE + 11)
#define DIALOG_ID_APPLY_BUTTON (DIALOG_ID_BASE + 12)

// Forward declarations
typedef struct DialogTheme DialogTheme;
typedef struct DialogResources DialogResources;
typedef struct BaseDialog BaseDialog;
typedef struct DialogConfig DialogConfig;

// Event callback types
typedef void (*DialogCallback)(BaseDialog* dialog, int event_type, void* data);
typedef void (*DialogCloseCallback)(BaseDialog* dialog, int result);

// Theme structure
struct DialogTheme {
    COLORREF bg_color;
    COLORREF text_color;
    COLORREF secondary_text_color;
    COLORREF accent_color;
    COLORREF border_color;
    COLORREF control_bg_color;
    COLORREF button_bg_color;
    COLORREF button_text_color;
    COLORREF button_hover_color;
    COLORREF button_pressed_color;
    bool is_dark_mode;
};

// Resource management
struct DialogResources {
    HFONT title_font;
    HFONT body_font;
    HFONT button_font;
    HBRUSH bg_brush;
    HBRUSH control_bg_brush;
    HBRUSH button_bg_brush;
    HBRUSH button_hover_brush;
    HPEN border_pen;
    HPEN accent_pen;
    HICON app_icon;
    bool initialized;
};

// Base dialog structure
struct BaseDialog {
    HWND hwnd;
    HWND parent_hwnd;
    DialogTheme theme;
    DialogResources resources;
    float dpi_scale;
    int width;
    int height;
    int result;
    bool modal;
    bool is_dragging;
    POINT drag_start;
    DialogCallback event_callback;
    DialogCloseCallback close_callback;
    void* user_data;
};

// Dialog configuration
struct DialogConfig {
    const char* title;
    const char* class_name;
    int width;
    int height;
    bool modal;
    bool resizable;
    HWND parent;
    DialogCallback event_callback;
    DialogCloseCallback close_callback;
    void* user_data;
};

// Dialog events
typedef enum {
    DIALOG_EVENT_CREATE,
    DIALOG_EVENT_DESTROY,
    DIALOG_EVENT_PAINT,
    DIALOG_EVENT_SIZE,
    DIALOG_EVENT_DPI_CHANGED,
    DIALOG_EVENT_THEME_CHANGED,
    DIALOG_EVENT_BUTTON_CLICK,
    DIALOG_EVENT_CLOSE
} DialogEventType;

// Button results
typedef enum {
    DIALOG_RESULT_CANCEL = 0,
    DIALOG_RESULT_OK = 1,
    DIALOG_RESULT_APPLY = 2,
    DIALOG_RESULT_YES = 3,
    DIALOG_RESULT_NO = 4
} DialogResult;

// Framework API

/**
 * Initialize the dialog framework
 * Must be called once before creating any dialogs
 */
bool dialog_framework_init(void);

/**
 * Cleanup the dialog framework
 * Should be called at application exit
 */
void dialog_framework_cleanup(void);

/**
 * Create a new base dialog
 * @param config Dialog configuration
 * @return Pointer to created dialog or NULL on failure
 */
BaseDialog* dialog_create(const DialogConfig* config);

/**
 * Show a modal dialog and wait for result
 * @param dialog Dialog to show
 * @return Dialog result code
 */
int dialog_show_modal(BaseDialog* dialog);

/**
 * Show a non-modal dialog
 * @param dialog Dialog to show
 * @return True on success
 */
bool dialog_show(BaseDialog* dialog);

/**
 * Close and destroy a dialog
 * @param dialog Dialog to destroy
 */
void dialog_destroy(BaseDialog* dialog);

/**
 * Get the current theme for the system
 * @param theme Output theme structure
 */
void dialog_get_system_theme(DialogTheme* theme);

/**
 * Update dialog resources for current DPI and theme
 * @param dialog Dialog to update
 */
void dialog_update_resources(BaseDialog* dialog);

/**
 * Paint the standard dialog background and header
 * @param dialog Dialog to paint
 * @param hdc Device context
 * @param title Title text to display
 */
void dialog_paint_background(BaseDialog* dialog, HDC hdc, const char* title);

/**
 * Handle standard dialog messages
 * @param dialog Dialog instance
 * @param msg Message
 * @param wParam wParam
 * @param lParam lParam
 * @return Message result, or -1 if not handled
 */
LRESULT dialog_handle_message(BaseDialog* dialog, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * Create standard dialog buttons
 * @param dialog Parent dialog
 * @param button_types Array of button types (DIALOG_RESULT_*)
 * @param button_count Number of buttons
 * @return True on success
 */
bool dialog_create_buttons(BaseDialog* dialog, const int* button_types, int button_count);

/**
 * Layout dialog buttons in bottom-right corner
 * @param dialog Dialog containing buttons
 */
void dialog_layout_buttons(BaseDialog* dialog);

#endif // DIALOG_FRAMEWORK_H