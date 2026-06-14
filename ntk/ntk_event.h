// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_EVENT_H
#define NTK_EVENT_H

#include "ntk_types.h"
typedef enum {
    NTK_EVENT_NONE           = 0,
    NTK_EVENT_MOUSE_PRESS    = 1,
    NTK_EVENT_MOUSE_RELEASE  = 2,
    NTK_EVENT_MOUSE_MOVE     = 3,
    NTK_EVENT_MOUSE_ENTER    = 4,
    NTK_EVENT_MOUSE_LEAVE    = 5,
    NTK_EVENT_KEY_PRESS      = 6,
    NTK_EVENT_KEY_RELEASE    = 7,
    NTK_EVENT_SCROLL         = 8,
    NTK_EVENT_FOCUS_IN       = 9,
    NTK_EVENT_FOCUS_OUT      = 10,
    NTK_EVENT_RESIZE         = 11,
    NTK_EVENT_DRAG           = 12,
    NTK_EVENT_DROP           = 13
} NtkEventType;
struct NtkEvent {
    NtkEventType    type;
    NtkWidget      *target;
    bool            propagates;
    bool            accepted;

    // Mouse data
    NtkPoint        mouse_pos;      // relative to target widget
    NtkPoint        global_pos;     // global coordinates
    NtkMouseButton  mouse_button;
    uint32_t        mouse_buttons;  // bitmask of pressed buttons

    // Key data
    NtkKeyCode      key_code;
    NtkModifiers    modifiers;
    uint32_t        codepoint;
    char            text[8];        // UTF-8 encoded text

    // Scroll data
    int             scroll_dx;
    int             scroll_dy;

    // Resize data
    int             width;
    int             height;
};
void        ntk_event_set_mouse_handler(NtkWidget *w, NtkMouseCallback cb, void *userdata);
void        ntk_event_set_key_handler(NtkWidget *w, NtkKeyCallback cb, void *userdata);
void        ntk_event_set_scroll_handler(NtkWidget *w, NtkScrollCallback cb, void *userdata);
void        ntk_event_set_focus_handler(NtkWidget *w, NtkFocusCallback cb, void *userdata);
void        ntk_event_set_resize_handler(NtkWidget *w, NtkResizeCallback cb, void *userdata);
void        ntk_event_set_enter_handler(NtkWidget *w, NtkCallback cb, void *userdata);
void        ntk_event_set_leave_handler(NtkWidget *w, NtkCallback cb, void *userdata);
void        ntk_event_set_drag_handler(NtkWidget *w, NtkDragCallback cb, void *userdata);
void        ntk_event_set_drop_handler(NtkWidget *w, NtkDropCallback cb, void *userdata);
bool        ntk_event_propagates(NtkEvent *e);
void        ntk_event_stop_propagation(NtkEvent *e);
NtkWidget*      ntk_event_get_target(NtkEvent *e);
NtkPoint        ntk_event_get_mouse_position(NtkEvent *e);
NtkMouseButton  ntk_event_get_mouse_button(NtkEvent *e);
NtkKeyCode      ntk_event_get_key_code(NtkEvent *e);
uint32_t        ntk_event_get_modifiers(NtkEvent *e);

#endif // NTK_EVENT_H
