// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

// BOREDOS_APP_DESC: Shows BoredOS information.

#include "ntk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BRANDING_PATH "/Library/images/branding/bOS_full_gradient_cropped.png"

typedef struct {
    char os_name[128];
    char os_version[128];
    char kernel_version[128];
} AboutState;

static void read_system_info(AboutState *st) {
    strcpy(st->os_name,        "BoredOS");
    strcpy(st->os_version,     "Unknown Version");
    strcpy(st->kernel_version, "Unknown Kernel");

    FILE *f = fopen("/proc/version", "r");
    if (!f) return;

    char v_buf[512];
    size_t bytes = fread(v_buf, 1, sizeof(v_buf) - 1, f);
    fclose(f);
    if (bytes == 0) return;
    v_buf[bytes] = '\0';

    char *l1 = v_buf;
    char *l2 = strchr(l1, '\n'); if (l2) { *l2++ = '\0'; } else l2 = NULL;
    char *l3 = l2 ? strchr(l2, '\n') : NULL; if (l3) { *l3++ = '\0'; }

    if (l1 && *l1) strncpy(st->os_name,        l1, sizeof(st->os_name) - 1);
    if (l2 && *l2) strncpy(st->os_version,     l2, sizeof(st->os_version) - 1);
    if (l3 && *l3) strncpy(st->kernel_version, l3, sizeof(st->kernel_version) - 1);
}

int main(void) {
    NtkApp *app = ntk_app_new();
    if (!app) return 1;

    AboutState state;
    read_system_info(&state);

    NtkWidget *win = ntk_window_new("About BoredOS", 420, 260);
    if (!win) {
        ntk_app_destroy(app);
        return 1;
    }
    ntk_window_set_resizable(win, false);

    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_box_set_spacing(vbox, 10);
    ntk_window_set_content(win, vbox);

    NtkWidget *img = ntk_image_new_from_file(BRANDING_PATH, vbox);
    if (img) {
        ntk_image_set_scale_mode(img, NTK_SCALE_FIT);
        ntk_widget_set_min_size(img, NTK_SIZE(360, 90));
        ntk_box_pack_start(vbox, img, false, false, 10);
    } else {
        NtkWidget *fallback_lbl = ntk_label_new("BoredOS", vbox);
        ntk_label_set_alignment(fallback_lbl, NTK_ALIGN_CENTER);
        ntk_box_pack_start(vbox, fallback_lbl, false, false, 20);
    }

    NtkWidget *lbl_name = ntk_label_new(state.os_name, vbox);
    ntk_label_set_alignment(lbl_name, NTK_ALIGN_CENTER);
    ntk_box_pack_start(vbox, lbl_name, false, false, 2);

    NtkWidget *lbl_ver = ntk_label_new(state.os_version, vbox);
    ntk_label_set_alignment(lbl_ver, NTK_ALIGN_CENTER);
    ntk_box_pack_start(vbox, lbl_ver, false, false, 2);

    NtkWidget *lbl_kern = ntk_label_new(state.kernel_version, vbox);
    ntk_label_set_alignment(lbl_kern, NTK_ALIGN_CENTER);
    ntk_box_pack_start(vbox, lbl_kern, false, false, 2);

    NtkWidget *sep = ntk_label_new("____________________________________________________", vbox);
    ntk_label_set_alignment(sep, NTK_ALIGN_CENTER);
    ntk_box_pack_start(vbox, sep, false, false, 5);

    NtkWidget *lbl_copyright = ntk_label_new("(C) 2023-2026 Christiaan (chris@boreddev.nl)", vbox);
    ntk_label_set_alignment(lbl_copyright, NTK_ALIGN_CENTER);
    ntk_box_pack_start(vbox, lbl_copyright, false, false, 2);

    NtkWidget *lbl_rights = ntk_label_new("All rights reserved.", vbox);
    ntk_label_set_alignment(lbl_rights, NTK_ALIGN_CENTER);
    ntk_box_pack_start(vbox, lbl_rights, false, false, 2);

    ntk_widget_show(win);
    int rc = ntk_app_run(app);

    ntk_app_destroy(app);
    return rc;
}
