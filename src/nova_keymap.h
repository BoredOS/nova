#ifndef NOVA_KEYMAP_H
#define NOVA_KEYMAP_H

#include <stdbool.h>
#include <stdint.h>

#include "libnovaproto/novaproto.h"

#define NOVA_KMOD_SHIFT  0x0001u
#define NOVA_KMOD_CTRL   0x0002u
#define NOVA_KMOD_ALT    0x0004u
#define NOVA_KMOD_ALTGR  0x0008u
#define NOVA_KMOD_CAPS   0x0010u

typedef struct {
    uint32_t normal;
    uint32_t shift;
    uint32_t altgr;
    uint32_t shift_altgr;
} NovaKeymapText;

typedef struct {
    const char *name;
    const char *aliases[4];
    NovaKeymapText text[128];
    NovaKeycode keycodes[128];
} NovaKeymap;

#define T(normal, shift, altgr, shift_altgr) \
    { (normal), (shift), (altgr), (shift_altgr) }

static const NovaKeymap nova_keymap_us = {
    .name = "us",
    .aliases = { "en", "qwerty", "english", NULL },
    .text = {
        [0x02] = T('1', '!', 0, 0),
        [0x03] = T('2', '@', 0, 0),
        [0x04] = T('3', '#', 0, 0),
        [0x05] = T('4', '$', 0, 0),
        [0x06] = T('5', '%', 0, 0),
        [0x07] = T('6', '^', 0, 0),
        [0x08] = T('7', '&', 0, 0),
        [0x09] = T('8', '*', 0, 0),
        [0x0A] = T('9', '(', 0, 0),
        [0x0B] = T('0', ')', 0, 0),
        [0x0C] = T('-', '_', 0, 0),
        [0x0D] = T('=', '+', 0, 0),
        [0x10] = T('q', 'Q', 0, 0),
        [0x11] = T('w', 'W', 0, 0),
        [0x12] = T('e', 'E', 0, 0),
        [0x13] = T('r', 'R', 0, 0),
        [0x14] = T('t', 'T', 0, 0),
        [0x15] = T('y', 'Y', 0, 0),
        [0x16] = T('u', 'U', 0, 0),
        [0x17] = T('i', 'I', 0, 0),
        [0x18] = T('o', 'O', 0, 0),
        [0x19] = T('p', 'P', 0, 0),
        [0x1A] = T('[', '{', 0, 0),
        [0x1B] = T(']', '}', 0, 0),
        [0x1E] = T('a', 'A', 0, 0),
        [0x1F] = T('s', 'S', 0, 0),
        [0x20] = T('d', 'D', 0, 0),
        [0x21] = T('f', 'F', 0, 0),
        [0x22] = T('g', 'G', 0, 0),
        [0x23] = T('h', 'H', 0, 0),
        [0x24] = T('j', 'J', 0, 0),
        [0x25] = T('k', 'K', 0, 0),
        [0x26] = T('l', 'L', 0, 0),
        [0x27] = T(';', ':', 0, 0),
        [0x28] = T('\'', '"', 0, 0),
        [0x29] = T('`', '~', 0, 0),
        [0x2B] = T('\\', '|', 0, 0),
        [0x2C] = T('z', 'Z', 0, 0),
        [0x2D] = T('x', 'X', 0, 0),
        [0x2E] = T('c', 'C', 0, 0),
        [0x2F] = T('v', 'V', 0, 0),
        [0x30] = T('b', 'B', 0, 0),
        [0x31] = T('n', 'N', 0, 0),
        [0x32] = T('m', 'M', 0, 0),
        [0x33] = T(',', '<', 0, 0),
        [0x34] = T('.', '>', 0, 0),
        [0x35] = T('/', '?', 0, 0),
        [0x39] = T(' ', ' ', 0, 0),
    },
    .keycodes = {
        [0x02] = KEY_1, [0x03] = KEY_2, [0x04] = KEY_3, [0x05] = KEY_4,
        [0x06] = KEY_5, [0x07] = KEY_6, [0x08] = KEY_7, [0x09] = KEY_8,
        [0x0A] = KEY_9, [0x0B] = KEY_0,
        [0x10] = KEY_Q, [0x11] = KEY_W, [0x12] = KEY_E, [0x13] = KEY_R,
        [0x14] = KEY_T, [0x15] = KEY_Y, [0x16] = KEY_U, [0x17] = KEY_I,
        [0x18] = KEY_O, [0x19] = KEY_P,
        [0x1E] = KEY_A, [0x1F] = KEY_S, [0x20] = KEY_D, [0x21] = KEY_F,
        [0x22] = KEY_G, [0x23] = KEY_H, [0x24] = KEY_J, [0x25] = KEY_K,
        [0x26] = KEY_L,
        [0x2C] = KEY_Z, [0x2D] = KEY_X, [0x2E] = KEY_C, [0x2F] = KEY_V,
        [0x30] = KEY_B, [0x31] = KEY_N, [0x32] = KEY_M,
    },
};

static const NovaKeymap nova_keymap_fr = {
    .name = "fr",
    .aliases = { "azerty", "fr-latin9", NULL },
    .text = {
        [0x02] = T('&', '1', 0, 0),
        [0x03] = T(0x00E9, '2', '~', 0),
        [0x04] = T('"', '3', '#', 0),
        [0x05] = T('\'', '4', '{', 0),
        [0x06] = T('(', '5', '[', 0),
        [0x07] = T('-', '6', '|', 0),
        [0x08] = T(0x00E8, '7', '`', 0),
        [0x09] = T('_', '8', '\\', 0),
        [0x0A] = T(0x00E7, '9', '^', 0),
        [0x0B] = T(0x00E0, '0', '@', 0),
        [0x0C] = T(')', 0x00B0, ']', 0),
        [0x0D] = T('=', '+', '}', 0),
        [0x10] = T('a', 'A', 0, 0),
        [0x11] = T('z', 'Z', 0, 0),
        [0x12] = T('e', 'E', 0x20AC, 0),
        [0x13] = T('r', 'R', 0, 0),
        [0x14] = T('t', 'T', 0, 0),
        [0x15] = T('y', 'Y', 0, 0),
        [0x16] = T('u', 'U', 0, 0),
        [0x17] = T('i', 'I', 0, 0),
        [0x18] = T('o', 'O', 0, 0),
        [0x19] = T('p', 'P', 0, 0),
        [0x1A] = T('^', 0x00A8, 0, 0),
        [0x1B] = T('$', 0x00A3, 0x00A4, 0),
        [0x1E] = T('q', 'Q', 0, 0),
        [0x1F] = T('s', 'S', 0, 0),
        [0x20] = T('d', 'D', 0, 0),
        [0x21] = T('f', 'F', 0, 0),
        [0x22] = T('g', 'G', 0, 0),
        [0x23] = T('h', 'H', 0, 0),
        [0x24] = T('j', 'J', 0, 0),
        [0x25] = T('k', 'K', 0, 0),
        [0x26] = T('l', 'L', 0, 0),
        [0x27] = T('m', 'M', 0, 0),
        [0x28] = T(0x00F9, '%', 0, 0),
        [0x29] = T(0x00B2, 0, 0, 0),
        [0x2B] = T('*', 0x00B5, 0, 0),
        [0x2C] = T('w', 'W', 0, 0),
        [0x2D] = T('x', 'X', 0, 0),
        [0x2E] = T('c', 'C', 0, 0),
        [0x2F] = T('v', 'V', 0, 0),
        [0x30] = T('b', 'B', 0, 0),
        [0x31] = T('n', 'N', 0, 0),
        [0x32] = T(',', '?', 0, 0),
        [0x33] = T(';', '.', 0, 0),
        [0x34] = T(':', '/', 0, 0),
        [0x35] = T('!', 0x00A7, 0, 0),
        [0x39] = T(' ', ' ', 0, 0),
    },
    .keycodes = {
        [0x02] = KEY_1, [0x03] = KEY_2, [0x04] = KEY_3, [0x05] = KEY_4,
        [0x06] = KEY_5, [0x07] = KEY_6, [0x08] = KEY_7, [0x09] = KEY_8,
        [0x0A] = KEY_9, [0x0B] = KEY_0,
        [0x10] = KEY_A, [0x11] = KEY_Z, [0x12] = KEY_E, [0x13] = KEY_R,
        [0x14] = KEY_T, [0x15] = KEY_Y, [0x16] = KEY_U, [0x17] = KEY_I,
        [0x18] = KEY_O, [0x19] = KEY_P,
        [0x1E] = KEY_Q, [0x1F] = KEY_S, [0x20] = KEY_D, [0x21] = KEY_F,
        [0x22] = KEY_G, [0x23] = KEY_H, [0x24] = KEY_J, [0x25] = KEY_K,
        [0x26] = KEY_L, [0x27] = KEY_M,
        [0x2C] = KEY_W, [0x2D] = KEY_X, [0x2E] = KEY_C, [0x2F] = KEY_V,
        [0x30] = KEY_B, [0x31] = KEY_N,
    },
};

static const NovaKeymap nova_keymap_de = {
    .name = "de",
    .aliases = { "qwertz", "german", NULL },
    .text = {
        [0x02] = T('1', '!', 0, 0),
        [0x03] = T('2', '"', 0x00B2, 0),
        [0x04] = T('3', 0x00A7, 0x00B3, 0),
        [0x05] = T('4', '$', 0, 0),
        [0x06] = T('5', '%', 0, 0),
        [0x07] = T('6', '&', 0, 0),
        [0x08] = T('7', '/', '{', 0),
        [0x09] = T('8', '(', '[', 0),
        [0x0A] = T('9', ')', ']', 0),
        [0x0B] = T('0', '=', '}', 0),
        [0x0C] = T(0x00DF, '?', '\\', 0),
        [0x0D] = T(0x00B4, '`', 0, 0),
        [0x10] = T('q', 'Q', '@', 0),
        [0x11] = T('w', 'W', 0, 0),
        [0x12] = T('e', 'E', 0x20AC, 0),
        [0x13] = T('r', 'R', 0, 0),
        [0x14] = T('t', 'T', 0, 0),
        [0x15] = T('z', 'Z', 0, 0),
        [0x16] = T('u', 'U', 0, 0),
        [0x17] = T('i', 'I', 0, 0),
        [0x18] = T('o', 'O', 0, 0),
        [0x19] = T('p', 'P', 0, 0),
        [0x1A] = T(0x00FC, 0x00DC, 0, 0),
        [0x1B] = T('+', '*', '~', 0),
        [0x1E] = T('a', 'A', 0, 0),
        [0x1F] = T('s', 'S', 0, 0),
        [0x20] = T('d', 'D', 0, 0),
        [0x21] = T('f', 'F', 0, 0),
        [0x22] = T('g', 'G', 0, 0),
        [0x23] = T('h', 'H', 0, 0),
        [0x24] = T('j', 'J', 0, 0),
        [0x25] = T('k', 'K', 0, 0),
        [0x26] = T('l', 'L', 0, 0),
        [0x27] = T(0x00F6, 0x00D6, 0, 0),
        [0x28] = T(0x00E4, 0x00C4, 0, 0),
        [0x29] = T('^', 0x00B0, 0, 0),
        [0x2B] = T('#', '\'', 0, 0),
        [0x2C] = T('y', 'Y', 0, 0),
        [0x2D] = T('x', 'X', 0, 0),
        [0x2E] = T('c', 'C', 0, 0),
        [0x2F] = T('v', 'V', 0, 0),
        [0x30] = T('b', 'B', 0, 0),
        [0x31] = T('n', 'N', 0, 0),
        [0x32] = T('m', 'M', 0, 0),
        [0x33] = T(',', ';', 0, 0),
        [0x34] = T('.', ':', 0, 0),
        [0x35] = T('-', '_', 0, 0),
        [0x39] = T(' ', ' ', 0, 0),
    },
    .keycodes = {
        [0x02] = KEY_1, [0x03] = KEY_2, [0x04] = KEY_3, [0x05] = KEY_4,
        [0x06] = KEY_5, [0x07] = KEY_6, [0x08] = KEY_7, [0x09] = KEY_8,
        [0x0A] = KEY_9, [0x0B] = KEY_0,
        [0x10] = KEY_Q, [0x11] = KEY_W, [0x12] = KEY_E, [0x13] = KEY_R,
        [0x14] = KEY_T, [0x15] = KEY_Z, [0x16] = KEY_U, [0x17] = KEY_I,
        [0x18] = KEY_O, [0x19] = KEY_P,
        [0x1E] = KEY_A, [0x1F] = KEY_S, [0x20] = KEY_D, [0x21] = KEY_F,
        [0x22] = KEY_G, [0x23] = KEY_H, [0x24] = KEY_J, [0x25] = KEY_K,
        [0x26] = KEY_L,
        [0x2C] = KEY_Y, [0x2D] = KEY_X, [0x2E] = KEY_C, [0x2F] = KEY_V,
        [0x30] = KEY_B, [0x31] = KEY_N, [0x32] = KEY_M,
    },
};

static const NovaKeymap nova_keymap_dvorak = {
    .name = "dvorak",
    .aliases = { "us-dvorak", NULL },
    .text = {
        [0x02] = T('1', '!', 0, 0),
        [0x03] = T('2', '@', 0, 0),
        [0x04] = T('3', '#', 0, 0),
        [0x05] = T('4', '$', 0, 0),
        [0x06] = T('5', '%', 0, 0),
        [0x07] = T('6', '^', 0, 0),
        [0x08] = T('7', '&', 0, 0),
        [0x09] = T('8', '*', 0, 0),
        [0x0A] = T('9', '(', 0, 0),
        [0x0B] = T('0', ')', 0, 0),
        [0x0C] = T('[', '{', 0, 0),
        [0x0D] = T(']', '}', 0, 0),
        [0x10] = T('\'', '"', 0, 0),
        [0x11] = T(',', '<', 0, 0),
        [0x12] = T('.', '>', 0, 0),
        [0x13] = T('p', 'P', 0, 0),
        [0x14] = T('y', 'Y', 0, 0),
        [0x15] = T('f', 'F', 0, 0),
        [0x16] = T('g', 'G', 0, 0),
        [0x17] = T('c', 'C', 0, 0),
        [0x18] = T('r', 'R', 0, 0),
        [0x19] = T('l', 'L', 0, 0),
        [0x1A] = T('/', '?', 0, 0),
        [0x1B] = T('=', '+', 0, 0),
        [0x1E] = T('a', 'A', 0, 0),
        [0x1F] = T('o', 'O', 0, 0),
        [0x20] = T('e', 'E', 0, 0),
        [0x21] = T('u', 'U', 0, 0),
        [0x22] = T('i', 'I', 0, 0),
        [0x23] = T('d', 'D', 0, 0),
        [0x24] = T('h', 'H', 0, 0),
        [0x25] = T('t', 'T', 0, 0),
        [0x26] = T('n', 'N', 0, 0),
        [0x27] = T('s', 'S', 0, 0),
        [0x28] = T('-', '_', 0, 0),
        [0x29] = T('`', '~', 0, 0),
        [0x2B] = T('\\', '|', 0, 0),
        [0x2C] = T(';', ':', 0, 0),
        [0x2D] = T('q', 'Q', 0, 0),
        [0x2E] = T('j', 'J', 0, 0),
        [0x2F] = T('k', 'K', 0, 0),
        [0x30] = T('x', 'X', 0, 0),
        [0x31] = T('b', 'B', 0, 0),
        [0x32] = T('m', 'M', 0, 0),
        [0x33] = T('w', 'W', 0, 0),
        [0x34] = T('v', 'V', 0, 0),
        [0x35] = T('z', 'Z', 0, 0),
        [0x39] = T(' ', ' ', 0, 0),
    },
    .keycodes = {
        [0x02] = KEY_1, [0x03] = KEY_2, [0x04] = KEY_3, [0x05] = KEY_4,
        [0x06] = KEY_5, [0x07] = KEY_6, [0x08] = KEY_7, [0x09] = KEY_8,
        [0x0A] = KEY_9, [0x0B] = KEY_0,
        [0x13] = KEY_P, [0x14] = KEY_Y, [0x15] = KEY_F, [0x16] = KEY_G,
        [0x17] = KEY_C, [0x18] = KEY_R, [0x19] = KEY_L,
        [0x1E] = KEY_A, [0x1F] = KEY_O, [0x20] = KEY_E, [0x21] = KEY_U,
        [0x22] = KEY_I, [0x23] = KEY_D, [0x24] = KEY_H, [0x25] = KEY_T,
        [0x26] = KEY_N, [0x27] = KEY_S,
        [0x2D] = KEY_Q, [0x2E] = KEY_J, [0x2F] = KEY_K, [0x30] = KEY_X,
        [0x31] = KEY_B, [0x32] = KEY_M, [0x33] = KEY_W, [0x34] = KEY_V,
        [0x35] = KEY_Z,
    },
};

static const NovaKeymap *const nova_keymaps[] = {
    &nova_keymap_us,
    &nova_keymap_fr,
    &nova_keymap_de,
    &nova_keymap_dvorak,
};

#undef T

static bool nova_keymap_streq_ci(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = *a++;
        char cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return false;
    }
    return *a == '\0' && *b == '\0';
}

static const NovaKeymap *nova_keymap_default(void) {
    return &nova_keymap_us;
}

static const NovaKeymap *nova_keymap_find(const char *name) {
    if (!name || !*name) return nova_keymap_default();

    for (size_t i = 0; i < sizeof(nova_keymaps) / sizeof(nova_keymaps[0]); i++) {
        const NovaKeymap *map = nova_keymaps[i];
        if (nova_keymap_streq_ci(name, map->name)) return map;
        for (size_t j = 0; j < sizeof(map->aliases) / sizeof(map->aliases[0]); j++) {
            if (!map->aliases[j]) break;
            if (nova_keymap_streq_ci(name, map->aliases[j])) return map;
        }
    }
    return NULL;
}

static NovaKeycode nova_keymap_keycode(const NovaKeymap *map, uint8_t scancode) {
    if (!map) map = nova_keymap_default();
    if (scancode >= 128) return KEY_UNKNOWN;
    return map->keycodes[scancode];
}

static bool nova_keymap_is_alpha_key(NovaKeycode key) {
    return key >= KEY_A && key <= KEY_Z;
}

static uint32_t nova_keymap_codepoint(const NovaKeymap *map, uint8_t scancode, uint32_t modifiers) {
    if (!map) map = nova_keymap_default();
    if (scancode >= 128) return 0;

    if ((modifiers & NOVA_KMOD_CTRL) && !(modifiers & NOVA_KMOD_ALTGR)) {
        return 0;
    }

    const NovaKeymapText *entry = &map->text[scancode];
    bool shift = (modifiers & NOVA_KMOD_SHIFT) != 0;
    bool altgr = (modifiers & NOVA_KMOD_ALTGR) != 0;
    bool caps = (modifiers & NOVA_KMOD_CAPS) != 0;

    if (altgr) {
        if (shift && entry->shift_altgr) return entry->shift_altgr;
        return entry->altgr;
    }

    NovaKeycode key = nova_keymap_keycode(map, scancode);
    if (caps && nova_keymap_is_alpha_key(key)) {
        return shift ? entry->normal : (entry->shift ? entry->shift : entry->normal);
    }

    if (shift && entry->shift) return entry->shift;
    return entry->normal;
}

#endif
