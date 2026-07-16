// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_nova.h"
#include "ntk_event.h"
#include "libnovaproto/novaproto.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

int ntk_nova_connect(const char *socket_path) {
    return nova_connect(socket_path);
}

void ntk_nova_disconnect(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

int ntk_nova_create_surface(int fd, uint32_t w, uint32_t h, uint8_t layer, uint32_t flags, uint32_t *surf_id_out, char *shm_path_out) {
    return nova_create_surface(fd, w, h, layer, flags, surf_id_out, shm_path_out);
}

int ntk_nova_resize_surface(int fd, uint32_t surf_id, uint32_t w, uint32_t h, char *new_shm_path_out) {
    return nova_resize_surface(fd, surf_id, w, h, new_shm_path_out);
}

int ntk_nova_damage_surface(int fd, uint32_t surf_id, int rect_count, const NtkRect *rects) {
    return nova_damage_surface(fd, surf_id, rect_count, (const NovaRect *)rects);
}

int ntk_nova_move_surface(int fd, uint32_t surf_id, int x, int y) {
    return nova_move_surface(fd, surf_id, x, y);
}

int ntk_nova_destroy_surface(int fd, uint32_t surf_id) {
    return nova_destroy_surface(fd, surf_id);
}

int ntk_nova_set_title(int fd, uint32_t surf_id, const char *title) {
    return nova_set_title(fd, surf_id, title);
}

int ntk_nova_set_icon(int fd, uint32_t surf_id, const char *icon_path) {
    return nova_set_icon(fd, surf_id, icon_path);
}

int ntk_nova_set_state(int fd, uint32_t surf_id, uint32_t state_flags) {
    return nova_set_state(fd, surf_id, state_flags);
}

int ntk_nova_set_flags(int fd, uint32_t surf_id, uint32_t flags) {
    return nova_set_flags(fd, surf_id, flags);
}

int ntk_nova_query_windows(int fd) {
    return nova_query_windows(fd);
}

int ntk_nova_poll_event(int fd, NtkEvent *event_out) {
    if (!event_out) return -1;
    NovaEvent ne;
    int rc = nova_poll_event(fd, &ne);
    if (rc < 0) return rc;
    memset(event_out, 0, sizeof(NtkEvent));
    event_out->accepted = true;
    event_out->propagates = true;
    event_out->target = (NtkWidget *)(uintptr_t)ne.surface_id;
    switch (ne.type) {
        case EVT_KEY:
            event_out->type = ne.data.key.pressed ? NTK_EVENT_KEY_PRESS : NTK_EVENT_KEY_RELEASE;
            event_out->key_code = ne.data.key.keycode;
            event_out->modifiers = ne.data.key.modifiers;
            event_out->codepoint = ne.data.key.codepoint;
            strncpy(event_out->text, ne.data.key.text, 7);
            event_out->text[7] = '\0';
            break;
        case EVT_POINTER:
            event_out->type = NTK_EVENT_MOUSE_MOVE;
            event_out->mouse_pos = NTK_POINT(ne.data.pointer.x, ne.data.pointer.y);
            event_out->global_pos = event_out->mouse_pos;
            event_out->mouse_buttons = ne.data.pointer.buttons;
            break;
        case EVT_SCROLL:
            event_out->type = NTK_EVENT_SCROLL;
            event_out->mouse_pos = NTK_POINT(ne.data.scroll.x, ne.data.scroll.y);
            event_out->global_pos = event_out->mouse_pos;
            event_out->scroll_dx = ne.data.scroll.dx;
            event_out->scroll_dy = ne.data.scroll.dy;
            break;
        case EVT_RESIZE_REQUEST:
            event_out->type = NTK_EVENT_RESIZE;
            event_out->width = ne.data.resize.w;
            event_out->height = ne.data.resize.h;
            break;
        case EVT_CLOSE_REQUEST:
            event_out->type = (NtkEventType)ne.type; 
            break;
        case EVT_FOCUS_IN:
            event_out->type = NTK_EVENT_FOCUS_IN;
            break;
        case EVT_FOCUS_OUT:
            event_out->type = NTK_EVENT_FOCUS_OUT;
            break;
        default:
            event_out->type = (NtkEventType)ne.type;
            break;
    }

    return 0;
}

int ntk_nova_pending_events(void) {
    return nova_pending_events();
}

NtkSize ntk_nova_get_screen_size(int fd) {
    (void)fd;
    NtkSize size = NTK_SIZE(1024, 768); 
    
    struct fb_var_screeninfo vinfo;
    int fb = open("/dev/fb0", O_RDONLY);
    if (fb >= 0) {
        if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) == 0) {
            size.width = vinfo.xres;
            size.height = vinfo.yres;
        }
        close(fb);
    }
    return size;
}
