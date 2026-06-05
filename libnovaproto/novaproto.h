#ifndef NOVAPROTO_H
#define NOVAPROTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NOVA_MAGIC 0x4E4F5641 // 'NOVA'
#define NOVA_VERSION 1.1

// Surface Creation Flags
#define SURFACE_FLAG_TRANSPARENT 0x1
#define SURFACE_FLAG_NO_RESIZE    0x2

// Message Types (Client -> Nova)
#define MSG_CREATE_SURFACE      1
#define MSG_RESIZE_SURFACE      2
#define MSG_MOVE_SURFACE        3
#define MSG_DAMAGE              4
#define MSG_SET_TITLE           5
#define MSG_DESTROY_SURFACE     6
#define MSG_SET_INPUT_FOCUS     7
#define MSG_QUERY_SCREEN        8
#define MSG_SET_STATE           9
#define MSG_SET_ICON            10
#define MSG_QUERY_WINDOWS       11
#define MSG_SET_FLAGS           12
#define MSG_QUIT                13

// Event Types (Nova -> Client)
#define EVT_KEY                 101
#define EVT_POINTER             102
#define EVT_RESIZE_REQUEST      103
#define EVT_CLOSE_REQUEST       104
#define EVT_FOCUS_IN            105
#define EVT_FOCUS_OUT           106
#define EVT_THEME_UPDATE        107
#define EVT_STATE_CHANGED       108
#define EVT_WINDOW_CREATED      109
#define EVT_WINDOW_DESTROYED    110
#define EVT_WINDOW_TITLE_CHANGED 111
#define EVT_WINDOW_LIST_END     112

// Nova Virtual Keycode Enum
typedef enum {
    KEY_UNKNOWN = 0,
    KEY_A = 1, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J,
    KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T,
    KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
    KEY_ENTER, KEY_ESCAPE, KEY_BACKSPACE, KEY_TAB, KEY_SPACE,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_LSHIFT, KEY_RSHIFT, KEY_LCTRL, KEY_RCTRL, KEY_LALT, KEY_RALT
} NovaKeycode;

// Rect structure
typedef struct {
    int x, y;
    uint32_t w, h;
} NovaRect;

// Event structure sent to Client
typedef struct {
    uint32_t type;
    uint32_t surface_id;
    union {
        struct { int x, y; uint32_t buttons; } pointer; // Coordinates relative to top-left of content area
        struct {
            uint32_t keycode;
            uint32_t modifiers;
            uint8_t pressed;
            uint8_t text_len;
            char text[5];
            uint32_t codepoint;
        } key;
        struct { uint32_t w, h; } resize;
        struct { uint32_t state_flags; } state;
        struct { char title[128]; uint32_t state_flags; char icon_path[256]; } window;
    } data;
} NovaEvent;

// Window Info returned on Query
typedef struct {
    uint32_t surface_id;
    char title[128];
    uint32_t state_flags;
    char icon_path[256];
} NovaWindowInfo;

// Wire Frame Header
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t flags;
    uint32_t msg_type;
    uint32_t payload_size;
} __attribute__((packed)) NovaFrameHeader;

// API functions
int nova_connect(const char *socket_path);
int nova_create_surface(int fd, uint32_t w, uint32_t h, uint8_t layer, uint32_t flags, uint32_t *surf_id_out, char *shm_path_out);
int nova_damage_surface(int fd, uint32_t surf_id, int rect_count, const NovaRect *rects);
int nova_move_surface(int fd, uint32_t surf_id, int x, int y);
int nova_resize_surface(int fd, uint32_t surf_id, uint32_t w, uint32_t h, char *new_shm_path_out);
int nova_set_state(int fd, uint32_t surf_id, uint32_t state_flags);
int nova_set_flags(int fd, uint32_t surf_id, uint32_t flags);
int nova_set_icon(int fd, uint32_t surf_id, const char *icon_path);
int nova_set_title(int fd, uint32_t surf_id, const char *title);
int nova_destroy_surface(int fd, uint32_t surf_id);
int nova_query_windows(int fd);
int nova_quit(int fd);
int nova_poll_event(int fd, NovaEvent *event_out);
int nova_pending_events(void);

#endif
