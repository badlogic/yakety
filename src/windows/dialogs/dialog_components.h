#ifndef DIALOG_COMPONENTS_H
#define DIALOG_COMPONENTS_H

#include "dialog_framework.h"
#include "dialog_utils.h"

// Component base structure
typedef struct DialogComponent DialogComponent;

// Component types
typedef enum {
    COMPONENT_TYPE_HEADER,
    COMPONENT_TYPE_BUTTON,
    COMPONENT_TYPE_LIST,
    COMPONENT_TYPE_PROGRESS,
    COMPONENT_TYPE_CAPTURE_AREA
} ComponentType;

// Component state
typedef struct DialogComponent {
    ComponentType type;
    HWND hwnd;
    HWND parent;
    BaseDialog* dialog;
    RECT bounds;
    bool visible;
    bool enabled;
    void* data;
    void (*paint_func)(DialogComponent* component, HDC hdc);
    void (*click_func)(DialogComponent* component, POINT pt);
    void (*cleanup_func)(DialogComponent* component);
} DialogComponent;

// Header Component
typedef struct {
    char* title;
    HICON icon;
    HFONT title_font;
    bool show_icon;
} HeaderComponentData;

/**
 * Create header component with app icon and title
 * @param dialog Parent dialog
 * @param title Header title text
 * @param bounds Component bounds
 * @return Component pointer or NULL on failure
 */
DialogComponent* header_component_create(BaseDialog* dialog, const char* title, const RECT* bounds);

/**
 * Update header title
 * @param component Header component
 * @param title New title text
 */
void header_component_set_title(DialogComponent* component, const char* title);

// Button Component
typedef struct {
    char* text;
    int button_id;
    bool is_default;
    bool is_hovered;
    bool is_pressed;
    HFONT font;
    void (*click_callback)(DialogComponent* component, int button_id);
} ButtonComponentData;

/**
 * Create themed button component
 * @param dialog Parent dialog
 * @param text Button text
 * @param button_id Button identifier
 * @param bounds Component bounds
 * @param is_default True if default button
 * @param click_callback Click handler
 * @return Component pointer or NULL on failure
 */
DialogComponent* button_component_create(BaseDialog* dialog, const char* text, int button_id,
                                        const RECT* bounds, bool is_default,
                                        void (*click_callback)(DialogComponent*, int));

/**
 * Set button hover state
 * @param component Button component
 * @param hovered True if hovered
 */
void button_component_set_hover(DialogComponent* component, bool hovered);

/**
 * Set button pressed state
 * @param component Button component
 * @param pressed True if pressed
 */
void button_component_set_pressed(DialogComponent* component, bool pressed);

// List Component (for scrollable model list)
typedef struct ListItem ListItem;

struct ListItem {
    char* title;
    char* description;
    char* detail;
    char* status_text;
    COLORREF status_color;
    bool selected;
    bool has_delete_button;
    void* user_data;
    ListItem* next;
};

typedef struct {
    ListItem* items;
    ListItem* selected_item;
    int item_count;
    int visible_items;
    int scroll_offset;
    int item_height;
    HFONT title_font;
    HFONT body_font;
    void (*selection_callback)(DialogComponent* component, ListItem* item);
    void (*delete_callback)(DialogComponent* component, ListItem* item);
} ListComponentData;

/**
 * Create scrollable list component
 * @param dialog Parent dialog
 * @param bounds Component bounds
 * @param item_height Height of each list item
 * @param selection_callback Item selection handler
 * @return Component pointer or NULL on failure
 */
DialogComponent* list_component_create(BaseDialog* dialog, const RECT* bounds, int item_height,
                                      void (*selection_callback)(DialogComponent*, ListItem*));

/**
 * Add item to list
 * @param component List component
 * @param title Item title
 * @param description Item description
 * @param detail Item detail text (e.g., size)
 * @param status_text Status badge text
 * @param status_color Status badge color
 * @param user_data User data pointer
 * @return Added item or NULL on failure
 */
ListItem* list_component_add_item(DialogComponent* component, const char* title,
                                 const char* description, const char* detail,
                                 const char* status_text, COLORREF status_color,
                                 void* user_data);

/**
 * Clear all list items
 * @param component List component
 */
void list_component_clear(DialogComponent* component);

/**
 * Get selected list item
 * @param component List component
 * @return Selected item or NULL
 */
ListItem* list_component_get_selected(DialogComponent* component);

/**
 * Set delete button callback
 * @param component List component
 * @param delete_callback Delete handler
 */
void list_component_set_delete_callback(DialogComponent* component,
                                       void (*delete_callback)(DialogComponent*, ListItem*));

// Progress Component
typedef struct {
    float progress;     // 0.0 to 1.0
    char* status_text;
    HFONT font;
    bool animate;
    COLORREF progress_color;
    COLORREF bg_color;
} ProgressComponentData;

/**
 * Create progress bar component
 * @param dialog Parent dialog
 * @param bounds Component bounds
 * @return Component pointer or NULL on failure
 */
DialogComponent* progress_component_create(BaseDialog* dialog, const RECT* bounds);

/**
 * Set progress value
 * @param component Progress component
 * @param progress Progress value (0.0 to 1.0)
 */
void progress_component_set_progress(DialogComponent* component, float progress);

/**
 * Set progress status text
 * @param component Progress component
 * @param status_text Status text
 */
void progress_component_set_status(DialogComponent* component, const char* status_text);

/**
 * Enable/disable progress animation
 * @param component Progress component
 * @param animate True to enable animation
 */
void progress_component_set_animate(DialogComponent* component, bool animate);

// Capture Area Component (for hotkey capture)
typedef struct {
    char* captured_text;
    bool is_capturing;
    bool has_focus;
    HFONT font;
    void* capture_data;
    void (*capture_callback)(DialogComponent* component, void* capture_data);
} CaptureAreaComponentData;

/**
 * Create key capture area component
 * @param dialog Parent dialog
 * @param bounds Component bounds
 * @param capture_callback Capture completion handler
 * @return Component pointer or NULL on failure
 */
DialogComponent* capture_area_component_create(BaseDialog* dialog, const RECT* bounds,
                                              void (*capture_callback)(DialogComponent*, void*));

/**
 * Start key capture
 * @param component Capture area component
 */
void capture_area_component_start_capture(DialogComponent* component);

/**
 * Stop key capture
 * @param component Capture area component
 */
void capture_area_component_stop_capture(DialogComponent* component);

/**
 * Set captured text display
 * @param component Capture area component
 * @param text Text to display
 */
void capture_area_component_set_text(DialogComponent* component, const char* text);

// Generic Component Functions

/**
 * Destroy a component and free its resources
 * @param component Component to destroy
 */
void dialog_component_destroy(DialogComponent* component);

/**
 * Set component visibility
 * @param component Component
 * @param visible True to show, false to hide
 */
void dialog_component_set_visible(DialogComponent* component, bool visible);

/**
 * Set component enabled state
 * @param component Component
 * @param enabled True to enable, false to disable
 */
void dialog_component_set_enabled(DialogComponent* component, bool enabled);

/**
 * Move component to new position
 * @param component Component
 * @param bounds New bounds
 */
void dialog_component_set_bounds(DialogComponent* component, const RECT* bounds);

/**
 * Paint component
 * @param component Component to paint
 * @param hdc Device context
 */
void dialog_component_paint(DialogComponent* component, HDC hdc);

/**
 * Handle component click
 * @param component Component
 * @param pt Click point in component coordinates
 */
void dialog_component_click(DialogComponent* component, POINT pt);

#endif // DIALOG_COMPONENTS_H