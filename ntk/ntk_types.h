// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_TYPES_H
#define NTK_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <poll.h>
typedef struct { int x, y; } NtkPoint;
typedef struct { int width, height; } NtkSize;
typedef struct { int x, y, width, height; } NtkRect;
typedef struct { int top, right, bottom, left; } NtkInsets;
typedef uint32_t NtkColor;
typedef enum {
    NTK_HORIZONTAL = 0,
    NTK_VERTICAL   = 1
} NtkOrientation;

typedef enum {
    NTK_ALIGN_START  = 0,
    NTK_ALIGN_CENTER = 1,
    NTK_ALIGN_END    = 2,
    NTK_ALIGN_FILL   = 3
} NtkAlign;

typedef enum {
    NTK_SCROLL_ALWAYS = 0,
    NTK_SCROLL_NEVER  = 1,
    NTK_SCROLL_AUTO   = 2
} NtkScrollPolicy;

typedef enum {
    NTK_SELECTION_SINGLE   = 0,
    NTK_SELECTION_MULTIPLE = 1,
    NTK_SELECTION_NONE     = 2
} NtkSelectionMode;

typedef enum {
    NTK_SORT_ASCENDING  = 0,
    NTK_SORT_DESCENDING = 1
} NtkSortOrder;

typedef enum {
    NTK_WRAP_NONE = 0,
    NTK_WRAP_WORD = 1,
    NTK_WRAP_CHAR = 2
} NtkWrapMode;

typedef enum {
    NTK_ELLIPSIZE_NONE   = 0,
    NTK_ELLIPSIZE_START  = 1,
    NTK_ELLIPSIZE_MIDDLE = 2,
    NTK_ELLIPSIZE_END    = 3
} NtkEllipsizeMode;

typedef enum {
    NTK_SCALE_FIT     = 0,
    NTK_SCALE_FILL    = 1,
    NTK_SCALE_STRETCH = 2,
    NTK_SCALE_NONE    = 3
} NtkScaleMode;

typedef enum {
    NTK_CURSOR_DEFAULT    = 0,
    NTK_CURSOR_ARROW      = 1,
    NTK_CURSOR_IBEAM      = 2,
    NTK_CURSOR_CROSSHAIR  = 3,
    NTK_CURSOR_HAND       = 4,
    NTK_CURSOR_RESIZE_H   = 5,
    NTK_CURSOR_RESIZE_V   = 6,
    NTK_CURSOR_RESIZE_TL  = 7,
    NTK_CURSOR_RESIZE_TR  = 8,
    NTK_CURSOR_MOVE       = 9,
    NTK_CURSOR_WAIT       = 10,
    NTK_CURSOR_FORBIDDEN  = 11
} NtkCursor;

typedef enum {
    NTK_CHECK_UNCHECKED     = 0,
    NTK_CHECK_CHECKED       = 1,
    NTK_CHECK_INDETERMINATE = 2
} NtkCheckState;

typedef enum {
    NTK_TAB_TOP    = 0,
    NTK_TAB_BOTTOM = 1,
    NTK_TAB_LEFT   = 2,
    NTK_TAB_RIGHT  = 3
} NtkTabPosition;

typedef enum {
    NTK_STACK_TRANSITION_NONE       = 0,
    NTK_STACK_TRANSITION_CROSSFADE  = 1,
    NTK_STACK_TRANSITION_SLIDE_LEFT = 2,
    NTK_STACK_TRANSITION_SLIDE_RIGHT = 3,
    NTK_STACK_TRANSITION_SLIDE_UP   = 4,
    NTK_STACK_TRANSITION_SLIDE_DOWN = 5
} NtkStackTransition;

typedef enum {
    NTK_TICKS_NONE   = 0,
    NTK_TICKS_LEFT   = 1,
    NTK_TICKS_RIGHT  = 2,
    NTK_TICKS_ABOVE  = 3,
    NTK_TICKS_BELOW  = 4,
    NTK_TICKS_BOTH   = 5
} NtkTickPosition;

typedef enum {
    NTK_ICON_POS_LEFT   = 0,
    NTK_ICON_POS_RIGHT  = 1,
    NTK_ICON_POS_TOP    = 2,
    NTK_ICON_POS_BOTTOM = 3
} NtkIconPosition;

typedef enum {
    NTK_ICON_TEXT_BOTTOM = 0,
    NTK_ICON_TEXT_RIGHT  = 1,
    NTK_ICON_TEXT_TOP    = 2,
    NTK_ICON_TEXT_LEFT   = 3
} NtkIconTextPosition;

typedef enum {
    NTK_GRADIENT_PAD     = 0,
    NTK_GRADIENT_REPEAT  = 1,
    NTK_GRADIENT_REFLECT = 2
} NtkGradientSpread;

typedef enum {
    NTK_PIXEL_ARGB32 = 0,
    NTK_PIXEL_RGBA32 = 1,
    NTK_PIXEL_RGB24  = 2
} NtkPixelFormat;

typedef enum {
    NTK_FONT_WEIGHT_THIN       = 100,
    NTK_FONT_WEIGHT_LIGHT      = 300,
    NTK_FONT_WEIGHT_NORMAL     = 400,
    NTK_FONT_WEIGHT_MEDIUM     = 500,
    NTK_FONT_WEIGHT_BOLD       = 700,
    NTK_FONT_WEIGHT_EXTRA_BOLD = 800,
    NTK_FONT_WEIGHT_BLACK      = 900
} NtkFontWeight;

typedef enum {
    NTK_FONT_STYLE_NORMAL  = 0,
    NTK_FONT_STYLE_ITALIC  = 1,
    NTK_FONT_STYLE_OBLIQUE = 2
} NtkFontStyle;

typedef enum {
    NTK_TOOLBAR_ICONS_ONLY = 0,
    NTK_TOOLBAR_TEXT_ONLY  = 1,
    NTK_TOOLBAR_ICONS_TEXT = 2
} NtkToolBarStyle;

typedef enum {
    NTK_BUTTON_ORDER_CLOSE_RIGHT = 0,
    NTK_BUTTON_ORDER_CLOSE_LEFT  = 1
} NtkButtonOrder;

typedef enum {
    NTK_MSG_INFO     = 0,
    NTK_MSG_QUESTION = 1,
    NTK_MSG_WARNING  = 2,
    NTK_MSG_ERROR    = 3
} NtkMessageType;

typedef enum {
    NTK_DIALOG_OK             = 0,
    NTK_DIALOG_CANCEL         = 1,
    NTK_DIALOG_YES            = 2,
    NTK_DIALOG_NO             = 3,
    NTK_DIALOG_CLOSE          = 4,
    NTK_DIALOG_RESULT_UNKNOWN = -1
} NtkDialogResult;

typedef enum {
    NTK_DIALOG_BUTTONS_OK              = 0,
    NTK_DIALOG_BUTTONS_OK_CANCEL       = 1,
    NTK_DIALOG_BUTTONS_YES_NO          = 2,
    NTK_DIALOG_BUTTONS_YES_NO_CANCEL   = 3
} NtkDialogButtons;

typedef enum {
    NTK_MOUSE_BUTTON_NONE   = 0,
    NTK_MOUSE_BUTTON_LEFT   = 1,
    NTK_MOUSE_BUTTON_MIDDLE = 2,
    NTK_MOUSE_BUTTON_RIGHT  = 3
} NtkMouseButton;
typedef enum {
    NTK_STYLE_ROLE_WINDOW_BG = 0,
    NTK_STYLE_ROLE_PANEL_BG,
    NTK_STYLE_ROLE_WIDGET_BG,
    NTK_STYLE_ROLE_WIDGET_BG_HOVER,
    NTK_STYLE_ROLE_WIDGET_BG_ACTIVE,
    NTK_STYLE_ROLE_WIDGET_BG_DISABLED,
    NTK_STYLE_ROLE_WIDGET_BORDER,
    NTK_STYLE_ROLE_TEXT_PRIMARY,
    NTK_STYLE_ROLE_TEXT_SECONDARY,
    NTK_STYLE_ROLE_TEXT_DISABLED,
    NTK_STYLE_ROLE_TEXT_ERROR,
    NTK_STYLE_ROLE_ACCENT,
    NTK_STYLE_ROLE_SELECTION_BG,
    NTK_STYLE_ROLE_SELECTION_TEXT,
    NTK_STYLE_ROLE_TITLEBAR_ACTIVE_START,
    NTK_STYLE_ROLE_TITLEBAR_ACTIVE_END,
    NTK_STYLE_ROLE_TITLEBAR_INACTIVE_START,
    NTK_STYLE_ROLE_TITLEBAR_INACTIVE_END,
    NTK_STYLE_ROLE_TITLEBAR_TEXT_ACTIVE,
    NTK_STYLE_ROLE_TITLEBAR_TEXT_INACTIVE,
    NTK_STYLE_ROLE_BORDER_LIGHT,
    NTK_STYLE_ROLE_BORDER_DARK,
    NTK_STYLE_ROLE_MENUBAR_BG,
    NTK_STYLE_ROLE_MENU_BG,
    NTK_STYLE_ROLE_MENU_HIGHLIGHT,
    NTK_STYLE_ROLE_TOOLBAR_BG,
    NTK_STYLE_ROLE_STATUSBAR_BG,
    NTK_STYLE_ROLE_SCROLLBAR_BG,
    NTK_STYLE_ROLE_SCROLLBAR_THUMB,
    NTK_STYLE_ROLE_COUNT
} NtkStyleRole;

typedef enum {
    NTK_STYLE_ELEMENT_DEFAULT_FONT = 0,
    NTK_STYLE_ELEMENT_TITLE_FONT,
    NTK_STYLE_ELEMENT_MENU_FONT,
    NTK_STYLE_ELEMENT_STATUS_FONT,
    NTK_STYLE_ELEMENT_MONO_FONT,
    NTK_STYLE_ELEMENT_COUNT
} NtkStyleElement;

typedef enum {
    NTK_STYLE_METRIC_BORDER_WIDTH = 0,
    NTK_STYLE_METRIC_BORDER_RADIUS,
    NTK_STYLE_METRIC_PADDING,
    NTK_STYLE_METRIC_SPACING,
    NTK_STYLE_METRIC_TITLEBAR_HEIGHT,
    NTK_STYLE_METRIC_MENUBAR_HEIGHT,
    NTK_STYLE_METRIC_TOOLBAR_HEIGHT,
    NTK_STYLE_METRIC_STATUSBAR_HEIGHT,
    NTK_STYLE_METRIC_SCROLLBAR_WIDTH,
    NTK_STYLE_METRIC_BUTTON_HEIGHT,
    NTK_STYLE_METRIC_ENTRY_HEIGHT,
    NTK_STYLE_METRIC_COUNT
} NtkStyleMetric;

typedef enum {
    NTK_STYLE_PIXMAP_CLOSE_NORMAL = 0,
    NTK_STYLE_PIXMAP_CLOSE_HOVER,
    NTK_STYLE_PIXMAP_CLOSE_PRESSED,
    NTK_STYLE_PIXMAP_MINIMIZE_NORMAL,
    NTK_STYLE_PIXMAP_MINIMIZE_HOVER,
    NTK_STYLE_PIXMAP_MINIMIZE_PRESSED,
    NTK_STYLE_PIXMAP_MAXIMIZE_NORMAL,
    NTK_STYLE_PIXMAP_MAXIMIZE_HOVER,
    NTK_STYLE_PIXMAP_MAXIMIZE_PRESSED,
    NTK_STYLE_PIXMAP_CHECKBOX_UNCHECKED,
    NTK_STYLE_PIXMAP_CHECKBOX_CHECKED,
    NTK_STYLE_PIXMAP_CHECKBOX_INDETERMINATE,
    NTK_STYLE_PIXMAP_RADIO_UNSELECTED,
    NTK_STYLE_PIXMAP_RADIO_SELECTED,
    NTK_STYLE_PIXMAP_ARROW_UP,
    NTK_STYLE_PIXMAP_ARROW_DOWN,
    NTK_STYLE_PIXMAP_ARROW_LEFT,
    NTK_STYLE_PIXMAP_ARROW_RIGHT,
    NTK_STYLE_PIXMAP_COUNT
} NtkStylePixmap;

typedef enum {
    NTK_STYLE_GRADIENT_TITLEBAR_ACTIVE = 0,
    NTK_STYLE_GRADIENT_TITLEBAR_INACTIVE,
    NTK_STYLE_GRADIENT_BUTTON_NORMAL,
    NTK_STYLE_GRADIENT_BUTTON_HOVER,
    NTK_STYLE_GRADIENT_BUTTON_PRESSED,
    NTK_STYLE_GRADIENT_PROGRESS_FILL,
    NTK_STYLE_GRADIENT_COUNT
} NtkStyleGradientRole;
typedef uint32_t NtkKeyCode;
typedef uint32_t NtkModifiers;

typedef struct {
    NtkKeyCode   key;
    NtkModifiers modifiers;
} NtkKeySequence;
#define NTK_MOD_SHIFT   0x01
#define NTK_MOD_CTRL    0x02
#define NTK_MOD_ALT     0x04
#define NTK_MOD_ALTGR   0x08
#define NTK_MOD_CAPS    0x10
typedef struct {
    float m[3][3];
} NtkTransform;

typedef struct {
    const char *description;  
    const char *pattern;      
} NtkFileFilter;

typedef struct NtkWidget       NtkWidget;
typedef struct NtkContainer    NtkContainer;
typedef struct NtkWindow       NtkWindow;
typedef struct NtkTitleBar     NtkTitleBar;
typedef struct NtkBox          NtkBox;
typedef struct NtkGrid         NtkGrid;
typedef struct NtkScrollArea   NtkScrollArea;
typedef struct NtkViewport     NtkViewport;
typedef struct NtkScrollBar    NtkScrollBar;
typedef struct NtkTabWidget    NtkTabWidget;
typedef struct NtkTabPage      NtkTabPage;
typedef struct NtkStack        NtkStack;
typedef struct NtkSplitter     NtkSplitter;
typedef struct NtkGroupBox     NtkGroupBox;
typedef struct NtkLabel        NtkLabel;
typedef struct NtkButton       NtkButton;
typedef struct NtkToggleButton NtkToggleButton;
typedef struct NtkCheckBox     NtkCheckBox;
typedef struct NtkRadioButton  NtkRadioButton;
typedef struct NtkRadioGroup   NtkRadioGroup;
typedef struct NtkTextEntry    NtkTextEntry;
typedef struct NtkTextArea     NtkTextArea;
typedef struct NtkSpinBox      NtkSpinBox;
typedef struct NtkSlider       NtkSlider;
typedef struct NtkComboBox     NtkComboBox;
typedef struct NtkListBox      NtkListBox;
typedef struct NtkTreeView     NtkTreeView;
typedef struct NtkTreeNode     NtkTreeNode;
typedef struct NtkTableView    NtkTableView;
typedef struct NtkIconView     NtkIconView;
typedef struct NtkProgressBar  NtkProgressBar;
typedef struct NtkMenuBar      NtkMenuBar;
typedef struct NtkMenu         NtkMenu;
typedef struct NtkMenuItem     NtkMenuItem;
typedef struct NtkToolBar      NtkToolBar;
typedef struct NtkToolButton   NtkToolButton;
typedef struct NtkStatusBar    NtkStatusBar;
typedef struct NtkImage        NtkImage;
typedef struct NtkCanvas       NtkCanvas;
typedef struct NtkDialog       NtkDialog;
typedef struct NtkPainter      NtkPainter;
typedef struct NtkStyle        NtkStyle;
typedef struct NtkTitleBarStyle NtkTitleBarStyle;
typedef struct NtkGradient     NtkGradient;
typedef struct NtkPixmap       NtkPixmap;
typedef struct NtkFont         NtkFont;
typedef struct NtkApp          NtkApp;
typedef struct NtkEvent        NtkEvent;

// Nova compositor types
typedef struct NovaCompositor  NovaCompositor;
typedef struct NovaScreen      NovaScreen;
typedef struct NovaSurface     NovaSurface;
typedef struct NovaTaskBar     NovaTaskBar;
typedef struct NovaTrayArea    NovaTrayArea;

typedef enum {
    NOVA_LAYER_BACKGROUND = 0,
    NOVA_LAYER_DESKTOP    = 1,
    NOVA_LAYER_NORMAL     = 2,
    NOVA_LAYER_ABOVE      = 3,
    NOVA_LAYER_DIALOG     = 4,
    NOVA_LAYER_PANEL      = 5,
    NOVA_LAYER_OVERLAY    = 6
} NovaLayer;

typedef enum {
    NOVA_WALLPAPER_CENTER  = 0,
    NOVA_WALLPAPER_STRETCH = 1,
    NOVA_WALLPAPER_TILE    = 2,
    NOVA_WALLPAPER_FIT     = 3,
    NOVA_WALLPAPER_FILL    = 4
} NovaWallpaperMode;

typedef void (*NtkCallback)(NtkWidget *widget, void *userdata);
typedef void (*NtkMouseCallback)(NtkWidget *widget, NtkEvent *event, void *userdata);
typedef void (*NtkKeyCallback)(NtkWidget *widget, NtkEvent *event, void *userdata);
typedef void (*NtkScrollCallback)(NtkWidget *widget, NtkEvent *event, void *userdata);
typedef void (*NtkFocusCallback)(NtkWidget *widget, bool focused, void *userdata);
typedef void (*NtkResizeCallback)(NtkWidget *widget, int width, int height, void *userdata);
typedef void (*NtkDragCallback)(NtkWidget *widget, NtkEvent *event, void *userdata);
typedef void (*NtkDropCallback)(NtkWidget *widget, NtkEvent *event, void *userdata);
typedef void (*NtkDrawCallback)(NtkCanvas *canvas, NtkPainter *painter, void *userdata);
typedef void (*NtkTimerCallback)(uint32_t timer_id, void *userdata);
typedef bool (*NtkInputFilter)(const char *text, uint32_t codepoint, void *userdata);
#define NTK_RECT(x, y, w, h)   ((NtkRect){(x), (y), (w), (h)})
#define NTK_POINT(x, y)        ((NtkPoint){(x), (y)})
#define NTK_SIZE(w, h)         ((NtkSize){(w), (h)})
#define NTK_INSETS(t, r, b, l) ((NtkInsets){(t), (r), (b), (l)})
#define NTK_INSETS_ALL(v)      ((NtkInsets){(v), (v), (v), (v)})
#define NTK_INSETS_ZERO        ((NtkInsets){0, 0, 0, 0})
#define NTK_SIZE_ZERO          ((NtkSize){0, 0})
#define NTK_POINT_ZERO         ((NtkPoint){0, 0})
#define NTK_RECT_ZERO          ((NtkRect){0, 0, 0, 0})

#endif // NTK_TYPES_H
