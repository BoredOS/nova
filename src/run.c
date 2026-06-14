// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.
#include "ntk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syscall.h>

static void copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) src = "";

    volatile const char *vsrc = (volatile const char *)src;
    size_t i = 0;
    while (i + 1 < dst_size) {
        char c = vsrc[i];
        if (!c) break;
        dst[i] = c;
        i++;
    }
    dst[i] = '\0';
}

static char *trim_spaces(char *str) {
    while (*str && (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n')) {
        str++;
    }
    if (*str == '\0') return str;

    char *end = str + strlen(str) - 1;
    while (end >= str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end-- = '\0';
    }
    return str;
}

static bool str_has_suffix(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    size_t len = strlen(str);
    size_t slen = strlen(suffix);
    if (len < slen) return false;
    return strcmp(str + len - slen, suffix) == 0;
}

static bool parse_exec_command(const char *cmd, char *out_exec, size_t exec_sz, char *out_args, size_t args_sz) {
    if (!cmd || !out_exec || exec_sz == 0) return false;
    copy_string(out_exec, exec_sz, cmd);
    if (out_args) out_args[0] = '\0';

    char *space = strchr(out_exec, ' ');
    if (space) {
        *space = '\0';
        if (out_args && args_sz > 0) {
            copy_string(out_args, args_sz, trim_spaces(space + 1));
        }
    }
    return true;
}

int main(void) {
    NtkApp *app = ntk_app_new();
    if (!app) return 1;

    char *cmd = ntk_dialog_get_text(NULL, "Run", "Type the name of a program to run:", "");
    if (cmd) {
        if (cmd[0] != '\0') {
            char *trimmed = trim_spaces(cmd);
            if (trimmed[0] != '\0') {
                char exec[256] = "";
                char args[256] = "";
                if (parse_exec_command(trimmed, exec, sizeof(exec), args, sizeof(args))) {
                    uint64_t flags = 0x2; // SPAWN_FLAG_INHERIT_TTY
                    int pid = sys_spawn(exec, args[0] ? args : NULL, flags, 0);
                    if (pid <= 0 && exec[0] != '/') {
                        char path[256];
                        snprintf(path, sizeof(path), "/bin/%s", exec);
                        pid = sys_spawn(path, args[0] ? args : NULL, flags, 0);
                        if (pid <= 0 && !str_has_suffix(exec, ".elf")) {
                            snprintf(path, sizeof(path), "/bin/%s.elf", exec);
                            sys_spawn(path, args[0] ? args : NULL, flags, 0);
                        }
                    }
                }
            }
        }
        free(cmd);
    }

    ntk_app_destroy(app);
    return 0;
}
