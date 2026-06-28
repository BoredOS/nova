// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_app.h"
#include "ntk_window.h"
#include "ntk_nova.h"
#include "ntk_widget.h"
#include "ntk_painter.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syscall.h>
#include <sys/time.h>
#include <errno.h>


// Struct definitions
typedef struct NtkTimer {
    uint32_t         id;
    uint32_t         interval;
    uint32_t         last_fire;
    NtkTimerCallback cb;
    void            *userdata;
    struct NtkTimer *next;
} NtkTimer;

struct NtkApp {
    int         fd;
    NtkWidget  *windows[32];
    int         window_count;
    NtkTimer   *timers;
    uint32_t    next_timer_id;
    bool        running;
    char       *clipboard;
};

// Global application instance
static NtkApp *g_app = NULL;

static uint32_t g_last_buttons = 0;
static NtkWidget *g_hovered_widget = NULL;
static NtkWidget *g_active_popup = NULL;

void ntk_app_set_active_popup(NtkWidget *popup) {
    g_active_popup = popup;
}

NtkWidget* ntk_app_get_active_popup(void) {
    return g_active_popup;
}

void ntk_app_notify_widget_destroyed(NtkWidget *w) {
    if (g_hovered_widget == w) {
        g_hovered_widget = NULL;
    }
    if (g_active_popup == w) {
        g_active_popup = NULL;
    }
}

// Helper to get ticks in milliseconds
static uint32_t ntk_get_ticks(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

NtkApp* ntk_app_new(void) {
    if (g_app) return g_app;

    NtkApp *app = calloc(1, sizeof(NtkApp));
    if (!app) return NULL;

    app->fd = ntk_nova_connect(NULL);
    if (app->fd < 0) {
        free(app);
        return NULL;
    }

    app->running = true;
    app->next_timer_id = 1;
    g_app = app;

    // Load default global style
    NtkStyle *def_style = ntk_style_new();
    if (def_style) {
        ntk_style_apply(def_style);
    }

    return app;
}

void ntk_app_destroy(NtkApp *app) {
    if (!app) return;

    // Destroy all windows
    while (app->window_count > 0) {
        ntk_window_close(app->windows[app->window_count - 1]);
    }

    // Free timers
    NtkTimer *t = app->timers;
    while (t) {
        NtkTimer *next = t->next;
        free(t);
        t = next;
    }

    ntk_nova_disconnect(app->fd);
    free(app->clipboard);
    free(app);

    if (g_app == app) {
        g_app = NULL;
    }
}

int ntk_app_get_fd(void) {
    return g_app ? g_app->fd : -1;
}

void ntk_app_register_window(NtkWidget *win) {
    if (!g_app || g_app->window_count >= 32) return;
    g_app->windows[g_app->window_count++] = win;
}

void ntk_app_unregister_window(NtkWidget *win) {
    if (!g_app) return;
    for (int i = 0; i < g_app->window_count; i++) {
        if (g_app->windows[i] == win) {
            for (int j = i; j < g_app->window_count - 1; j++) {
                g_app->windows[j] = g_app->windows[j + 1];
            }
            g_app->window_count--;
            return;
        }
    }
}

void ntk_app_quit(NtkApp *app) {
    if (app) app->running = false;
}

void ntk_app_set_clipboard(const char *text) {
    if (!g_app) return;
    free(g_app->clipboard);
    g_app->clipboard = text ? strdup(text) : NULL;
}

char* ntk_app_get_clipboard(void) {
    return g_app && g_app->clipboard ? strdup(g_app->clipboard) : NULL;
}

uint32_t ntk_app_set_timer(uint32_t interval_ms, NtkTimerCallback cb, void *userdata) {
    if (!g_app || !cb) return 0;

    NtkTimer *t = malloc(sizeof(NtkTimer));
    if (!t) return 0;

    t->id = g_app->next_timer_id++;
    t->interval = interval_ms;
    t->last_fire = ntk_get_ticks();
    t->cb = cb;
    t->userdata = userdata;
    t->next = g_app->timers;
    g_app->timers = t;

    return t->id;
}

void ntk_app_kill_timer(uint32_t timer_id) {
    if (!g_app) return;

    NtkTimer *curr = g_app->timers;
    NtkTimer *prev = NULL;

    while (curr) {
        if (curr->id == timer_id) {
            if (prev) prev->next = curr->next;
            else g_app->timers = curr->next;
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}
static NtkWidget* find_widget_at(NtkWidget *w, NtkPoint p) {
    int count = ntk_widget_get_child_count(w);
    for (int i = count - 1; i >= 0; i--) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child && ntk_widget_is_visible(child)) {
            NtkRect geom = ntk_widget_get_geometry(child);
            if (p.x >= geom.x && p.y >= geom.y &&
                p.x < geom.x + geom.width && p.y < geom.y + geom.height) {
                return find_widget_at(child, p);
            }
        }
    }
    return w;
}

static NtkWidget* find_window_child_at(NtkWidget *win, NtkPoint p) {
    NtkWidget *menubar = ntk_window_get_menubar(win);
    if (menubar && ntk_widget_is_visible(menubar)) {
        NtkRect geom = ntk_widget_get_geometry(menubar);
        if (p.x >= geom.x && p.y >= geom.y && p.x < geom.x + geom.width && p.y < geom.y + geom.height) {
            return find_widget_at(menubar, p);
        }
    }
    NtkWidget *toolbar = ntk_window_get_toolbar(win);
    if (toolbar && ntk_widget_is_visible(toolbar)) {
        NtkRect geom = ntk_widget_get_geometry(toolbar);
        if (p.x >= geom.x && p.y >= geom.y && p.x < geom.x + geom.width && p.y < geom.y + geom.height) {
            return find_widget_at(toolbar, p);
        }
    }
    NtkWidget *statusbar = ntk_window_get_statusbar(win);
    if (statusbar && ntk_widget_is_visible(statusbar)) {
        NtkRect geom = ntk_widget_get_geometry(statusbar);
        if (p.x >= geom.x && p.y >= geom.y && p.x < geom.x + geom.width && p.y < geom.y + geom.height) {
            return find_widget_at(statusbar, p);
        }
    }
    NtkWidget *content = ntk_window_get_content(win);
    if (content && ntk_widget_is_visible(content)) {
        NtkRect geom = ntk_widget_get_geometry(content);
        if (p.x >= geom.x && p.y >= geom.y && p.x < geom.x + geom.width && p.y < geom.y + geom.height) {
            return find_widget_at(content, p);
        }
    }
    return win;
}

static NtkWidget* find_focused_widget(NtkWidget *w) {
    if (ntk_widget_has_focus(w)) return w;
    int count = ntk_widget_get_child_count(w);
    for (int i = 0; i < count; i++) {
        NtkWidget *child = ntk_widget_get_child_at(w, i);
        if (child) {
            NtkWidget *f = find_focused_widget(child);
            if (f) return f;
        }
    }
    return NULL;
}

static void propagate_event(NtkWidget *target, NtkEvent *e) {
    NtkWidget *cur = target;
    e->target = target;
    NtkPoint global_pos = e->global_pos;

    while (cur) {
        e->mouse_pos = ntk_widget_map_from_global(cur, global_pos);
        const NtkWidgetClass *klass = ntk_widget_get_class(cur);
        
        bool accepted = false;
        if (klass && klass->handle_event) {
            accepted = klass->handle_event(cur, e);
        }
        
        if (accepted || !e->propagates) {
            break;
        }
        if (klass && strcmp(klass->type_name, "NtkWindow") == 0) {
            break;
        }
        cur = ntk_widget_get_parent(cur);
    }
}
static void app_draw_windows(void) {
    for (int i = 0; i < g_app->window_count; i++) {
        NtkWidget *win = g_app->windows[i];
        if (win) {
            NtkSize sz = ntk_widget_get_size(win);
            NtkPainter *p = ntk_painter_new(win);
            if (p) {
                const NtkWidgetClass *klass = ntk_widget_get_class(win);
                if (klass && klass->paint) {
                    klass->paint(win, p);
                }
                ntk_painter_destroy(p);
                
                extern uint32_t ntk_window_get_surf_id_internal(NtkWidget *w);
                uint32_t surf_id = ntk_window_get_surf_id_internal(win);
                NtkRect bounds = NTK_RECT(0, 0, sz.width, sz.height);
                ntk_nova_damage_surface(g_app->fd, surf_id, 1, &bounds);
            }
        }
    }
}

void ntk_app_draw_windows(void) {
    if (!g_app) return;
    for (int i = 0; i < g_app->window_count; i++) {
        NtkWidget *win = g_app->windows[i];
        const NtkWidgetClass *klass = ntk_widget_get_class(win);
        if (klass && klass->layout) klass->layout(win);
    }
    app_draw_windows();
}

static bool is_timer_valid(NtkTimer *timer) {
    if (!g_app) return false;
    NtkTimer *curr = g_app->timers;
    while (curr) {
        if (curr == timer) return true;
        curr = curr->next;
    }
    return false;
}

void ntk_app_run_modal(NtkWidget *modal_win, bool *done_flag) {
    if (!g_app) return;

    for (int i = 0; i < g_app->window_count; i++) {
        NtkWidget *win = g_app->windows[i];
        const NtkWidgetClass *klass = ntk_widget_get_class(win);
        if (klass && klass->layout) klass->layout(win);
    }
    app_draw_windows();

    struct pollfd pfd;
    pfd.fd     = g_app->fd;
    pfd.events = POLLIN;

    while (g_app->running && (done_flag ? !*done_flag : true)) {
        uint32_t now = ntk_get_ticks();
        uint32_t timeout = 16;
        bool timer_fired = false;

        NtkTimer *t = g_app->timers;
        while (t) {
            NtkTimer *next = NULL;
            uint32_t elapsed = now - t->last_fire;
            bool fired = false;
            if (elapsed >= t->interval) {
                t->cb(t->id, t->userdata);
                timer_fired = true;
                fired = true;
            }
            if (is_timer_valid(t)) {
                if (fired) {
                    t->last_fire = now;
                }
                uint32_t elapsed_after = now - t->last_fire;
                uint32_t remaining = (t->interval > elapsed_after) ? (t->interval - elapsed_after) : 0;
                if (remaining < timeout) timeout = remaining;
                next = t->next;
            } else {
                break;
            }
            t = next;
        }

        if (timer_fired) {
            app_draw_windows();
        }

        int pr = poll(&pfd, 1, (int)timeout);
        if (pr < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if ((pr > 0 && (pfd.revents & POLLIN)) || ntk_nova_pending_events()) {
            NtkEvent ev;
            while (ntk_nova_pending_events() || (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN))) {
                if (ntk_nova_poll_event(g_app->fd, &ev) < 0) {
                    g_app->running = false;
                    break;
                }

                uint32_t target_surf_id = (uint32_t)(uintptr_t)ev.target;
                NtkWidget *win = NULL;
                for (int i = 0; i < g_app->window_count; i++) {
                    extern uint32_t ntk_window_get_surf_id_internal(NtkWidget *w);
                    if (ntk_window_get_surf_id_internal(g_app->windows[i]) == target_surf_id) {
                        win = g_app->windows[i];
                        break;
                    }
                }

                if (!win) continue;

                if (g_active_popup && win != g_active_popup) {
                    if (ev.type == NTK_EVENT_MOUSE_MOVE ||
                        ev.type == NTK_EVENT_MOUSE_PRESS ||
                        ev.type == NTK_EVENT_MOUSE_RELEASE) {
                        if (ev.type == NTK_EVENT_MOUSE_PRESS || ev.mouse_buttons != 0) {
                            NtkWidget *pop = g_active_popup;
                            g_active_popup = NULL;
                            ntk_window_close(pop);
                        }
                        continue;
                    }
                }

                if (modal_win && win != modal_win) {
                    if (ev.type == NTK_EVENT_MOUSE_MOVE ||
                        ev.type == NTK_EVENT_MOUSE_PRESS ||
                        ev.type == NTK_EVENT_MOUSE_RELEASE ||
                        ev.type == NTK_EVENT_KEY_PRESS ||
                        ev.type == NTK_EVENT_KEY_RELEASE) {
                        continue;
                    }
                }

                if ((int)ev.type == 104) { 
                    ntk_window_close(win);
                    if (win == modal_win && done_flag) {
                        *done_flag = true;
                    }
                    if (g_app->window_count == 0) {
                        g_app->running = false;
                    }
                    continue;
                }

                if (ev.type == NTK_EVENT_RESIZE) {
                    const NtkWidgetClass *klass = ntk_widget_get_class(win);
                    if (klass && klass->handle_event) {
                        klass->handle_event(win, &ev);
                    }
                    app_draw_windows();
                    continue;
                }

                if (ev.type == NTK_EVENT_MOUSE_MOVE) {
                    NtkPoint pt = ev.mouse_pos;
                    ev.global_pos = pt; 

                    NtkWidget *target = find_window_child_at(win, pt);

                    if (target != g_hovered_widget) {
                        if (g_hovered_widget) {
                            NtkEvent leave_ev = ev;
                            leave_ev.type = NTK_EVENT_MOUSE_LEAVE;
                            leave_ev.target = g_hovered_widget;
                            const NtkWidgetClass *klass = ntk_widget_get_class(g_hovered_widget);
                            if (klass && klass->handle_event) klass->handle_event(g_hovered_widget, &leave_ev);
                        }
                        if (target) {
                            NtkEvent enter_ev = ev;
                            enter_ev.type = NTK_EVENT_MOUSE_ENTER;
                            enter_ev.target = target;
                            const NtkWidgetClass *klass = ntk_widget_get_class(target);
                            if (klass && klass->handle_event) klass->handle_event(target, &enter_ev);
                        }
                        g_hovered_widget = target;
                    }

                    uint32_t buttons = ev.mouse_buttons;
                    uint32_t diff = buttons ^ g_last_buttons;
                    if (diff) {
                        if (buttons & diff) {
                            ev.type = NTK_EVENT_MOUSE_PRESS;
                            if (diff & 1) ev.mouse_button = NTK_MOUSE_BUTTON_LEFT;
                            else if (diff & 2) ev.mouse_button = NTK_MOUSE_BUTTON_MIDDLE;
                            else if (diff & 4) ev.mouse_button = NTK_MOUSE_BUTTON_RIGHT;
                        } else {
                            ev.type = NTK_EVENT_MOUSE_RELEASE;
                            if (diff & 1) ev.mouse_button = NTK_MOUSE_BUTTON_LEFT;
                            else if (diff & 2) ev.mouse_button = NTK_MOUSE_BUTTON_MIDDLE;
                            else if (diff & 4) ev.mouse_button = NTK_MOUSE_BUTTON_RIGHT;
                        }
                    }
                    g_last_buttons = buttons;

                    propagate_event(target, &ev);
                } else if (ev.type == NTK_EVENT_KEY_PRESS || ev.type == NTK_EVENT_KEY_RELEASE) {
                    NtkWidget *focused = find_focused_widget(win);
                    if (!focused) focused = win;
                    propagate_event(focused, &ev);
                } else {
                    const NtkWidgetClass *klass = ntk_widget_get_class(win);
                    if (klass && klass->handle_event) klass->handle_event(win, &ev);
                }
            }

            app_draw_windows();
        }
    }
}

int ntk_app_run(NtkApp *app) {
    if (!app) return 1;
    ntk_app_run_modal(NULL, NULL);
    return 0;
}
