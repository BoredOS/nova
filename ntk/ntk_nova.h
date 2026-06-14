// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#ifndef NTK_NOVA_H
#define NTK_NOVA_H

#include "ntk_types.h"
int       ntk_nova_connect(const char *socket_path);
void      ntk_nova_disconnect(int fd);
int       ntk_nova_create_surface(int fd, uint32_t w, uint32_t h, uint8_t layer, uint32_t flags, uint32_t *surf_id_out, char *shm_path_out);
int       ntk_nova_resize_surface(int fd, uint32_t surf_id, uint32_t w, uint32_t h, char *new_shm_path_out);
int       ntk_nova_damage_surface(int fd, uint32_t surf_id, int rect_count, const NtkRect *rects);
int       ntk_nova_move_surface(int fd, uint32_t surf_id, int x, int y);
int       ntk_nova_destroy_surface(int fd, uint32_t surf_id);
int       ntk_nova_set_title(int fd, uint32_t surf_id, const char *title);
int       ntk_nova_set_icon(int fd, uint32_t surf_id, const char *icon_path);
int       ntk_nova_set_state(int fd, uint32_t surf_id, uint32_t state_flags);
int       ntk_nova_set_flags(int fd, uint32_t surf_id, uint32_t flags);
int       ntk_nova_query_windows(int fd);
int       ntk_nova_poll_event(int fd, NtkEvent *event_out);
int       ntk_nova_pending_events(void);
NtkSize   ntk_nova_get_screen_size(int fd);

#endif // NTK_NOVA_H
