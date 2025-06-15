# Windows Dialog Framework Refactor

## Overview

This document outlines the plan to refactor Windows dialogs into a reusable component framework that matches the macOS SwiftUI design exactly. The goal is to eliminate code duplication and create a system that makes implementing new dialogs straightforward.

## Current State Analysis

### Existing Implementation
- **Files**: `src/windows/dialog.c`, `src/windows/dialog_keycapture.c`
- **Status**: Basic dialogs working, but significant code duplication (~500+ lines)
- **Missing**: `dialog_models_and_language` and `dialog_model_download` (currently stubs)

### Code Duplication Issues
1. **Window Management**: Window class registration, DPI scaling, positioning (~100 lines duplicated)
2. **Theme System**: Dark mode detection, color schemes (~50 lines duplicated)
3. **Custom Buttons**: Button subclassing, hover states, painting (~200 lines duplicated)
4. **Title Bar**: Custom title bar drawing and dragging (~80 lines duplicated)
5. **Utilities**: UTF-8 conversion, font management (~70 lines duplicated)

## Target Design (macOS SwiftUI Reference)

### Visual Design Elements
- **Dark theme** with rounded corners and proper spacing
- **App icon + title header** with consistent 20px padding
- **Themed controls** with hover states and proper typography
- **Button layouts** with 12px spacing, 32px height
- **Card-based lists** with status badges (Bundled, Installed, Download Required)

### Dialog Specifications

#### 1. Models Dialog (520x400px)
- **Header**: App icon + "Select Model and Language" title
- **Language dropdown**: Full-width with blue accent
- **Model list**: Scrollable (278px height), card-based layout
- **Cards**: 100px height, rounded corners, selection states
- **Status badges**: Colored (blue=Bundled, green=Installed, orange=Download)
- **Actions**: Cancel + Apply buttons (bottom-right)

#### 2. Download Dialog (400x200px)
- **Header**: App icon + "Downloading [model name]" title
- **Progress bar**: Blue accent, 1.5x height scale, rounded
- **Status text**: "Downloaded X%" centered below bar
- **Actions**: Cancel button (disabled when complete)

#### 3. Hotkey Dialog (400x300px)
- **Header**: App icon + "Configure Hotkey" title
- **Instructions**: Centered explanatory text
- **Capture area**: 240x40px rounded input with focus state
- **Actions**: Cancel + OK buttons (OK disabled until capture)

## Proposed Framework Architecture

### File Structure
```
src/windows/dialogs/
├── dialog_framework.h         // Core framework definitions
├── dialog_framework.c         // Base dialog implementation
├── dialog_components.h        // Reusable UI components
├── dialog_components.c        // Component implementations
├── dialog_models.c           // Models dialog implementation
├── dialog_download.c         // Download dialog implementation
└── dialog_utils.c           // Shared utilities
```

### Core Components

#### 1. Base Dialog Framework
```c
typedef struct {
    HWND hwnd;
    HFONT header_font, body_font, button_font;
    HICON app_icon;
    DialogTheme theme;
    float dpi_scale;
    DialogResources resources;
} BaseDialog;

typedef struct {
    COLORREF bg_color;
    COLORREF text_color;
    COLORREF accent_color;
    COLORREF border_color;
    COLORREF control_bg_color;
    bool is_dark_mode;
} DialogTheme;
```

#### 2. Reusable Components
- **HeaderComponent**: App icon + title layout
- **ScrollableList**: Virtual scrolling with card rendering
- **ProgressBar**: Themed progress indicator with text
- **ThemedButton**: Custom-painted buttons with hover states
- **CaptureArea**: Key combination input area

#### 3. Utility Systems
- **DPIManager**: Scaling calculations and monitor handling
- **ThemeManager**: Dark mode detection and color schemes
- **ResourceManager**: Font/brush/pen lifecycle management

### Design Constants (Match SwiftUI)
```c
#define DIALOG_PADDING 20
#define DIALOG_HEADER_HEIGHT 76
#define DIALOG_BUTTON_HEIGHT 32
#define DIALOG_BUTTON_SPACING 12
#define LIST_ITEM_HEIGHT 100
#define SCROLLABLE_LIST_HEIGHT 278
#define CAPTURE_AREA_WIDTH 240
#define CAPTURE_AREA_HEIGHT 40
```

## Implementation Plan

### Phase 1: Foundation Framework
1. **Base Dialog System**
   - Extract common window creation/management
   - Implement theme detection and management
   - Create DPI scaling utilities
   - Build resource management system

2. **Core Components**
   - Header component with icon + title
   - Base button component with theming
   - Message area component

### Phase 2: Specialized Components
1. **Scrollable List Component**
   - Custom window class for list container
   - Virtual scrolling implementation
   - Card-based item rendering
   - Selection state management
   - Status badge rendering

2. **Progress Component**
   - Custom drawing with rounded corners
   - Animation support
   - Status text integration

3. **Input Components**
   - Key capture area with focus states
   - Dropdown/combobox theming

### Phase 3: Dialog Implementation
1. **Models Dialog**
   - Language dropdown integration
   - Model list with scrolling
   - Card selection and status display
   - Apply/cancel button handling

2. **Download Dialog**
   - Progress bar with status updates
   - Cancellation support
   - File management integration

3. **Hotkey Dialog Updates**
   - Migrate to new framework
   - Improve visual consistency

## Technical Considerations

### Win32 API Usage
- **Theming**: `DwmSetWindowAttribute` for dark mode, custom drawing for controls
- **DPI Scaling**: `GetDpiForMonitor` with Per-Monitor V2 awareness
- **Custom Controls**: Window subclassing and owner-draw patterns
- **Input Handling**: Low-level keyboard hooks for key capture

### Memory Management
- RAII-style resource management
- Explicit cleanup in `WM_DESTROY`
- Font/brush caching with DPI change handling
- Safe string conversion utilities

### Performance
- Virtual scrolling for large model lists
- Efficient repainting strategies
- Resource caching and reuse
- Minimize GDI object creation

## Reference Files

### Current Implementation
- `src/windows/dialog.c` - Basic dialog implementation with theming
- `src/windows/dialog_keycapture.c` - Key capture dialog with input handling
- `src/dialog.h` - Cross-platform dialog interface

### macOS Reference Implementation
- `src/mac/dialogs/models_dialog.swift` - Target design for models dialog
- `src/mac/dialogs/download_dialog.swift` - Target design for download dialog
- `src/mac/dialogs/hotkey_dialog.swift` - Target design for hotkey dialog
- `src/mac/dialogs/dialog_utils.swift` - SwiftUI modal dialog utilities

### Design Screenshots and References
- `docs/macos-models-dialog.png` - Models dialog screenshot (520x400px)
- `docs/macos-download-dialog.png` - Download dialog screenshot (400x200px)  
- `docs/macos-hotkey-dialog.png` - Hotkey dialog screenshot (400x300px)

#### Models Dialog Visual Specifications (from docs/macos-models-dialog.png)
- **Size**: 520x400px with dark theme
- **Header**: App icon (32x32) + "Select Model and Language" title
- **Language Section**: "Language" label + dropdown (full width) + help text
- **Model Section**: "Model" label + scrollable list (278px height)
- **Model Cards**: Rounded rectangles, 2px blue border when selected
  - Title in white, description in gray
  - Size info in small gray text
  - Status badges: "Bundled" (blue), "Installed" (green), delete button (red trash icon)
- **Buttons**: "Cancel" + "Apply" (bottom right, 12px spacing)
- **Layout**: 20px padding throughout, consistent spacing

## Implementation Tasks

### High Priority (Foundation)
- [ ] **Task 1**: Create dialog framework foundation (dialog_framework.h/c with base dialog system)
- [ ] **Task 2**: Extract common utilities into dialog_utils.c (DPI, theme, string conversion)

### Medium Priority (Components)
- [ ] **Task 3**: Build header component with app icon + title layout
- [ ] **Task 4**: Create themed button component with hover states
- [ ] **Task 5**: Implement scrollable list component for models dialog
- [ ] **Task 6**: Create progress bar component for download dialog

### Medium Priority (Dialog Implementation)
- [ ] **Task 7**: Implement models dialog using framework components
- [ ] **Task 8**: Implement download dialog using framework components

### Low Priority (Migration & Testing)
- [ ] **Task 9**: Migrate existing keycapture dialog to new framework
- [ ] **Task 10**: Test theme switching and DPI scaling across all dialogs

### Implementation Strategy
1. **Phase 1**: Complete tasks 1-2 (foundation framework and utilities)
2. **Phase 2**: Complete tasks 3-6 (reusable UI components)
3. **Phase 3**: Complete tasks 7-10 (dialog implementation and migration)

## Success Criteria

- [ ] All dialogs match macOS SwiftUI design exactly
- [ ] No code duplication between dialog implementations
- [ ] Easy to add new dialog types
- [ ] Proper dark mode and DPI scaling support
- [ ] Consistent behavior across all dialogs
- [ ] Performance equivalent to current implementation

---

*Last updated: 2025-06-15*
*Status: Planning phase - framework design complete*