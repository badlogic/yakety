#include "dialog_components.h"
#include "../logging.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations for component painters
static void header_component_paint(DialogComponent* component, HDC hdc);
static void header_component_cleanup(DialogComponent* component);
static void button_component_paint(DialogComponent* component, HDC hdc);
static void button_component_click(DialogComponent* component, POINT pt);
static void button_component_cleanup(DialogComponent* component);

// Generic Component Functions

DialogComponent* dialog_component_create(ComponentType type, BaseDialog* dialog, const RECT* bounds) {
    if (!dialog || !bounds) return NULL;

    DialogComponent* component = (DialogComponent*)calloc(1, sizeof(DialogComponent));
    if (!component) {
        log_error("Failed to allocate component memory");
        return NULL;
    }

    component->type = type;
    component->dialog = dialog;
    component->bounds = *bounds;
    component->visible = true;
    component->enabled = true;

    return component;
}

void dialog_component_destroy(DialogComponent* component) {
    if (!component) return;

    if (component->cleanup_func) {
        component->cleanup_func(component);
    }

    free(component);
}

void dialog_component_set_visible(DialogComponent* component, bool visible) {
    if (!component) return;
    
    component->visible = visible;
    if (component->hwnd) {
        ShowWindow(component->hwnd, visible ? SW_SHOW : SW_HIDE);
    }
}

void dialog_component_set_enabled(DialogComponent* component, bool enabled) {
    if (!component) return;
    
    component->enabled = enabled;
    if (component->hwnd) {
        EnableWindow(component->hwnd, enabled ? TRUE : FALSE);
    }
}

void dialog_component_set_bounds(DialogComponent* component, const RECT* bounds) {
    if (!component || !bounds) return;
    
    component->bounds = *bounds;
    if (component->hwnd) {
        SetWindowPos(component->hwnd, NULL, 
                    bounds->left, bounds->top, 
                    bounds->right - bounds->left, 
                    bounds->bottom - bounds->top,
                    SWP_NOZORDER);
    }
}

void dialog_component_paint(DialogComponent* component, HDC hdc) {
    if (!component || !component->visible || !component->paint_func) return;
    
    component->paint_func(component, hdc);
}

void dialog_component_click(DialogComponent* component, POINT pt) {
    if (!component || !component->visible || !component->enabled || !component->click_func) return;
    
    component->click_func(component, pt);
}

// Header Component Implementation

DialogComponent* header_component_create(BaseDialog* dialog, const char* title, const RECT* bounds) {
    DialogComponent* component = dialog_component_create(COMPONENT_TYPE_HEADER, dialog, bounds);
    if (!component) return NULL;

    HeaderComponentData* data = (HeaderComponentData*)calloc(1, sizeof(HeaderComponentData));
    if (!data) {
        dialog_component_destroy(component);
        return NULL;
    }

    // Set title
    if (title) {
        size_t title_len = strlen(title) + 1;
        data->title = (char*)malloc(title_len);
        if (data->title) {
            strcpy_s(data->title, title_len, title);
        }
    }

    // Create title font
    float dpi = dialog->dpi_scale;
    data->title_font = CreateFontW(
        dialog_scale_dpi(DIALOG_TITLE_FONT_SIZE, dpi), 0, 0, 0, FW_MEDIUM,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    // Get app icon
    int icon_size = dialog_scale_dpi(APP_ICON_SIZE, dpi);
    data->icon = (HICON)LoadImage(GetModuleHandle(NULL), MAKEINTRESOURCE(1), 
                                 IMAGE_ICON, icon_size, icon_size, 0);
    if (!data->icon) {
        data->icon = LoadIcon(NULL, IDI_APPLICATION);
    }
    data->show_icon = true;

    component->data = data;
    component->paint_func = header_component_paint;
    component->cleanup_func = header_component_cleanup;

    log_info("Created header component: %s", title ? title : "untitled");
    return component;
}

void header_component_set_title(DialogComponent* component, const char* title) {
    if (!component || component->type != COMPONENT_TYPE_HEADER) return;

    HeaderComponentData* data = (HeaderComponentData*)component->data;
    if (!data) return;

    // Free old title
    free(data->title);
    data->title = NULL;

    // Set new title
    if (title) {
        size_t title_len = strlen(title) + 1;
        data->title = (char*)malloc(title_len);
        if (data->title) {
            strcpy_s(data->title, title_len, title);
        }
    }
}

static void header_component_paint(DialogComponent* component, HDC hdc) {
    HeaderComponentData* data = (HeaderComponentData*)component->data;
    if (!data) return;

    RECT rect = component->bounds;
    BaseDialog* dialog = component->dialog;
    int padding = dialog_scale_dpi(DIALOG_PADDING, dialog->dpi_scale);
    int icon_size = dialog_scale_dpi(APP_ICON_SIZE, dialog->dpi_scale);

    // Draw icon
    if (data->show_icon && data->icon) {
        DrawIconEx(hdc, rect.left + padding, rect.top + padding, 
                  data->icon, icon_size, icon_size, 0, NULL, DI_NORMAL);
    }

    // Draw title
    if (data->title) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, dialog->theme.text_color);
        HFONT old_font = (HFONT)SelectObject(hdc, data->title_font);

        wchar_t* wide_title = dialog_utf8_to_wide(data->title);
        if (wide_title) {
            RECT title_rect = {
                rect.left + padding + (data->show_icon ? icon_size + padding/2 : 0),
                rect.top + padding,
                rect.right - padding,
                rect.top + padding + icon_size
            };
            DrawTextW(hdc, wide_title, -1, &title_rect, 
                     DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            free(wide_title);
        }

        SelectObject(hdc, old_font);
    }
}

static void header_component_cleanup(DialogComponent* component) {
    HeaderComponentData* data = (HeaderComponentData*)component->data;
    if (!data) return;

    free(data->title);
    if (data->title_font) DeleteObject(data->title_font);
    // Don't delete icon as it may be shared
    free(data);
}

// Button Component Implementation

DialogComponent* button_component_create(BaseDialog* dialog, const char* text, int button_id,
                                        const RECT* bounds, bool is_default,
                                        void (*click_callback)(DialogComponent*, int)) {
    DialogComponent* component = dialog_component_create(COMPONENT_TYPE_BUTTON, dialog, bounds);
    if (!component) return NULL;

    ButtonComponentData* data = (ButtonComponentData*)calloc(1, sizeof(ButtonComponentData));
    if (!data) {
        dialog_component_destroy(component);
        return NULL;
    }

    // Set text
    if (text) {
        size_t text_len = strlen(text) + 1;
        data->text = (char*)malloc(text_len);
        if (data->text) {
            strcpy_s(data->text, text_len, text);
        }
    }

    data->button_id = button_id;
    data->is_default = is_default;
    data->click_callback = click_callback;

    // Create button font
    float dpi = dialog->dpi_scale;
    data->font = CreateFontW(
        dialog_scale_dpi(DIALOG_BUTTON_FONT_SIZE, dpi), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    component->data = data;
    component->paint_func = button_component_paint;
    component->click_func = button_component_click;
    component->cleanup_func = button_component_cleanup;

    log_info("Created button component: %s (ID: %d)", text ? text : "untitled", button_id);
    return component;
}

void button_component_set_hover(DialogComponent* component, bool hovered) {
    if (!component || component->type != COMPONENT_TYPE_BUTTON) return;

    ButtonComponentData* data = (ButtonComponentData*)component->data;
    if (!data) return;

    if (data->is_hovered != hovered) {
        data->is_hovered = hovered;
        // Trigger repaint
        if (component->dialog && component->dialog->hwnd) {
            RECT rect = component->bounds;
            InvalidateRect(component->dialog->hwnd, &rect, FALSE);
        }
    }
}

void button_component_set_pressed(DialogComponent* component, bool pressed) {
    if (!component || component->type != COMPONENT_TYPE_BUTTON) return;

    ButtonComponentData* data = (ButtonComponentData*)component->data;
    if (!data) return;

    if (data->is_pressed != pressed) {
        data->is_pressed = pressed;
        // Trigger repaint
        if (component->dialog && component->dialog->hwnd) {
            RECT rect = component->bounds;
            InvalidateRect(component->dialog->hwnd, &rect, FALSE);
        }
    }
}

static void button_component_paint(DialogComponent* component, HDC hdc) {
    ButtonComponentData* data = (ButtonComponentData*)component->data;
    if (!data) return;

    RECT rect = component->bounds;
    BaseDialog* dialog = component->dialog;

    // Determine button colors based on state
    COLORREF bg_color = dialog->theme.button_bg_color;
    COLORREF text_color = dialog->theme.button_text_color;
    COLORREF border_color = dialog->theme.border_color;

    if (!component->enabled) {
        bg_color = dialog_darken_color(bg_color, 0.3f);
        text_color = dialog_darken_color(text_color, 0.5f);
    } else if (data->is_pressed) {
        bg_color = dialog->theme.button_pressed_color;
    } else if (data->is_hovered) {
        bg_color = dialog->theme.button_hover_color;
    }

    if (data->is_default) {
        border_color = dialog->theme.accent_color;
        if (!data->is_pressed && !data->is_hovered) {
            bg_color = dialog->theme.accent_color;
            text_color = RGB(255, 255, 255);
        }
    }

    // Draw button background with rounded corners
    HBRUSH bg_brush = CreateSolidBrush(bg_color);
    HPEN border_pen = CreatePen(PS_SOLID, 1, border_color);
    
    dialog_draw_rounded_rect(hdc, &rect, 4, border_pen, bg_brush);
    
    DeleteObject(bg_brush);
    DeleteObject(border_pen);

    // Draw button text
    if (data->text) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, text_color);
        HFONT old_font = (HFONT)SelectObject(hdc, data->font);

        wchar_t* wide_text = dialog_utf8_to_wide(data->text);
        if (wide_text) {
            // Add pressed offset
            RECT text_rect = rect;
            if (data->is_pressed) {
                OffsetRect(&text_rect, 1, 1);
            }
            
            DrawTextW(hdc, wide_text, -1, &text_rect, 
                     DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            free(wide_text);
        }

        SelectObject(hdc, old_font);
    }
}

static void button_component_click(DialogComponent* component, POINT pt) {
    ButtonComponentData* data = (ButtonComponentData*)component->data;
    if (!data || !data->click_callback) return;

    data->click_callback(component, data->button_id);
}

static void button_component_cleanup(DialogComponent* component) {
    ButtonComponentData* data = (ButtonComponentData*)component->data;
    if (!data) return;

    free(data->text);
    if (data->font) DeleteObject(data->font);
    free(data);
}

// TODO: Implement list_component_*, progress_component_*, and capture_area_component_* functions
// These will be implemented in the next phase