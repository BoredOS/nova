// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk_dialog.h"
#include "ntk_window.h"
#include "ntk_box.h"
#include "ntk_label.h"
#include "ntk_button.h"
#include "ntk_entry.h"
#include "ntk_spin.h"
#include "ntk_combo.h"
#include "ntk_font.h"
#include "ntk_color.h"
#include "ntk_app.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    bool            done;
    NtkDialogResult result;
} DialogState;

static void on_button_clicked(NtkWidget *w, void *userdata) {
    DialogState *state = userdata;
    int res = (int)(uintptr_t)ntk_widget_get_user_data(w, "result");
    state->result = (NtkDialogResult)res;
    state->done = true;
}

static void on_entry_activated(NtkWidget *w, void *userdata) {
    (void)w;
    DialogState *state = userdata;
    state->result = NTK_DIALOG_OK;
    state->done = true;
}

NtkDialogResult ntk_dialog_message(NtkWidget *parent, const char *title, const char *message, NtkMessageType type, NtkDialogButtons buttons) {
    (void)type;

    NtkWidget *win = ntk_window_new_dialog(parent, title, 360, 115);
    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_box_set_spacing(vbox, 15);
    ntk_window_set_content(win, vbox);

    NtkWidget *lbl = ntk_label_new(message, vbox);
    ntk_label_set_alignment(lbl, NTK_ALIGN_CENTER);
    ntk_box_pack_start(vbox, lbl, true, true, 10);

    NtkWidget *hbox = ntk_box_new(NTK_HORIZONTAL, vbox);
    ntk_box_set_spacing(hbox, 10);
    ntk_box_pack_start(vbox, hbox, false, false, 5);

    DialogState state = { .done = false, .result = NTK_DIALOG_RESULT_UNKNOWN };

    if (buttons == NTK_DIALOG_BUTTONS_OK) {
        NtkWidget *btn = ntk_button_new("OK", hbox);
        ntk_widget_set_user_data(btn, "result", (void*)(uintptr_t)NTK_DIALOG_OK);
        ntk_widget_connect(btn, "clicked", on_button_clicked, &state);
        ntk_box_pack_end(hbox, btn, false, false, 5);
    } else if (buttons == NTK_DIALOG_BUTTONS_OK_CANCEL) {
        NtkWidget *cancel = ntk_button_new("Cancel", hbox);
        ntk_widget_set_user_data(cancel, "result", (void*)(uintptr_t)NTK_DIALOG_CANCEL);
        ntk_widget_connect(cancel, "clicked", on_button_clicked, &state);
        ntk_box_pack_end(hbox, cancel, false, false, 5);

        NtkWidget *ok = ntk_button_new("OK", hbox);
        ntk_widget_set_user_data(ok, "result", (void*)(uintptr_t)NTK_DIALOG_OK);
        ntk_widget_connect(ok, "clicked", on_button_clicked, &state);
        ntk_box_pack_end(hbox, ok, false, false, 5);
    } else if (buttons == NTK_DIALOG_BUTTONS_YES_NO) {
        NtkWidget *no = ntk_button_new("No", hbox);
        ntk_widget_set_user_data(no, "result", (void*)(uintptr_t)NTK_DIALOG_NO);
        ntk_widget_connect(no, "clicked", on_button_clicked, &state);
        ntk_box_pack_end(hbox, no, false, false, 5);

        NtkWidget *yes = ntk_button_new("Yes", hbox);
        ntk_widget_set_user_data(yes, "result", (void*)(uintptr_t)NTK_DIALOG_YES);
        ntk_widget_connect(yes, "clicked", on_button_clicked, &state);
        ntk_box_pack_end(hbox, yes, false, false, 5);
    } else if (buttons == NTK_DIALOG_BUTTONS_YES_NO_CANCEL) {
        NtkWidget *cancel = ntk_button_new("Cancel", hbox);
        ntk_widget_set_user_data(cancel, "result", (void*)(uintptr_t)NTK_DIALOG_CANCEL);
        ntk_widget_connect(cancel, "clicked", on_button_clicked, &state);
        ntk_box_pack_end(hbox, cancel, false, false, 5);

        NtkWidget *no = ntk_button_new("No", hbox);
        ntk_widget_set_user_data(no, "result", (void*)(uintptr_t)NTK_DIALOG_NO);
        ntk_widget_connect(no, "clicked", on_button_clicked, &state);
        ntk_box_pack_end(hbox, no, false, false, 5);

        NtkWidget *yes = ntk_button_new("Yes", hbox);
        ntk_widget_set_user_data(yes, "result", (void*)(uintptr_t)NTK_DIALOG_YES);
        ntk_widget_connect(yes, "clicked", on_button_clicked, &state);
        ntk_box_pack_end(hbox, yes, false, false, 5);
    }

    ntk_widget_show(win);
    ntk_app_run_modal(win, &state.done);

    NtkDialogResult res = state.result;
    ntk_window_close(win);
    return res;
}

bool ntk_dialog_question(NtkWidget *parent, const char *title, const char *message) {
    NtkDialogResult res = ntk_dialog_message(parent, title, message, NTK_MSG_QUESTION, NTK_DIALOG_BUTTONS_YES_NO);
    return (res == NTK_DIALOG_YES);
}

void ntk_dialog_error(NtkWidget *parent, const char *title, const char *message) {
    ntk_dialog_message(parent, title, message, NTK_MSG_ERROR, NTK_DIALOG_BUTTONS_OK);
}

void ntk_dialog_warning(NtkWidget *parent, const char *title, const char *message) {
    ntk_dialog_message(parent, title, message, NTK_MSG_WARNING, NTK_DIALOG_BUTTONS_OK);
}

char* ntk_dialog_get_text(NtkWidget *parent, const char *title, const char *label, const char *initial_value) {
    NtkWidget *win = ntk_window_new_dialog(parent, title, 320, 115);
    if (title && strcmp(title, "Run") == 0) {
        ntk_window_set_icon_path(win, "/Library/Icons/serenityicons/16x16/app-run.png");
    }
    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_box_set_spacing(vbox, 10);
    ntk_window_set_content(win, vbox);

    NtkWidget *lbl = ntk_label_new(label, vbox);
    ntk_box_pack_start(vbox, lbl, false, false, 5);

    NtkWidget *entry = ntk_text_entry_new(vbox);
    if (initial_value) {
        ntk_text_entry_set_text(entry, initial_value);
    }
    ntk_box_pack_start(vbox, entry, false, false, 5);

    NtkWidget *hbox = ntk_box_new(NTK_HORIZONTAL, vbox);
    ntk_box_set_spacing(hbox, 10);
    ntk_box_pack_start(vbox, hbox, false, false, 5);

    NtkWidget *ok_btn = ntk_button_new("OK", hbox);
    NtkWidget *cancel_btn = ntk_button_new("Cancel", hbox);
    ntk_box_pack_end(hbox, cancel_btn, false, false, 5);
    ntk_box_pack_end(hbox, ok_btn, false, false, 5);

    DialogState state = { .done = false, .result = NTK_DIALOG_RESULT_UNKNOWN };
    ntk_widget_connect(entry, "activated", on_entry_activated, &state);
    ntk_widget_set_user_data(ok_btn, "result", (void*)(uintptr_t)NTK_DIALOG_OK);
    ntk_widget_set_user_data(cancel_btn, "result", (void*)(uintptr_t)NTK_DIALOG_CANCEL);
    ntk_widget_connect(ok_btn, "clicked", on_button_clicked, &state);
    ntk_widget_connect(cancel_btn, "clicked", on_button_clicked, &state);

    ntk_widget_show(win);
    ntk_app_run_modal(win, &state.done);

    char *ret = NULL;
    if (state.result == NTK_DIALOG_OK) {
        const char *txt = ntk_text_entry_get_text(entry);
        ret = txt ? strdup(txt) : strdup("");
    }

    ntk_window_close(win);
    return ret;
}

int ntk_dialog_get_integer(NtkWidget *parent, const char *title, const char *label, int initial_value, int min_val, int max_val, bool *ok) {
    NtkWidget *win = ntk_window_new_dialog(parent, title, 320, 115);
    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_box_set_spacing(vbox, 10);
    ntk_window_set_content(win, vbox);

    NtkWidget *lbl = ntk_label_new(label, vbox);
    ntk_box_pack_start(vbox, lbl, false, false, 5);

    NtkWidget *spin = ntk_spin_box_new(vbox);
    ntk_spin_box_set_range(spin, min_val, max_val);
    ntk_spin_box_set_value(spin, initial_value);
    ntk_box_pack_start(vbox, spin, false, false, 5);

    NtkWidget *hbox = ntk_box_new(NTK_HORIZONTAL, vbox);
    ntk_box_set_spacing(hbox, 10);
    ntk_box_pack_start(vbox, hbox, false, false, 5);

    NtkWidget *ok_btn = ntk_button_new("OK", hbox);
    NtkWidget *cancel_btn = ntk_button_new("Cancel", hbox);
    ntk_box_pack_end(hbox, cancel_btn, false, false, 5);
    ntk_box_pack_end(hbox, ok_btn, false, false, 5);

    DialogState state = { .done = false, .result = NTK_DIALOG_RESULT_UNKNOWN };
    ntk_widget_set_user_data(ok_btn, "result", (void*)(uintptr_t)NTK_DIALOG_OK);
    ntk_widget_set_user_data(cancel_btn, "result", (void*)(uintptr_t)NTK_DIALOG_CANCEL);
    ntk_widget_connect(ok_btn, "clicked", on_button_clicked, &state);
    ntk_widget_connect(cancel_btn, "clicked", on_button_clicked, &state);

    ntk_widget_show(win);
    ntk_app_run_modal(win, &state.done);

    int val = initial_value;
    if (state.result == NTK_DIALOG_OK) {
        val = ntk_spin_box_get_value(spin);
        if (ok) *ok = true;
    } else {
        if (ok) *ok = false;
    }

    ntk_window_close(win);
    return val;
}

double ntk_dialog_get_double(NtkWidget *parent, const char *title, const char *label, double initial_value, double min_val, double max_val, bool *ok) {
    char init_str[64];
    snprintf(init_str, sizeof(init_str), "%g", initial_value);

    NtkWidget *win = ntk_window_new_dialog(parent, title, 320, 115);
    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_box_set_spacing(vbox, 10);
    ntk_window_set_content(win, vbox);

    NtkWidget *lbl = ntk_label_new(label, vbox);
    ntk_box_pack_start(vbox, lbl, false, false, 5);

    NtkWidget *entry = ntk_text_entry_new(vbox);
    ntk_text_entry_set_text(entry, init_str);
    ntk_box_pack_start(vbox, entry, false, false, 5);

    NtkWidget *hbox = ntk_box_new(NTK_HORIZONTAL, vbox);
    ntk_box_set_spacing(hbox, 10);
    ntk_box_pack_start(vbox, hbox, false, false, 5);

    NtkWidget *ok_btn = ntk_button_new("OK", hbox);
    NtkWidget *cancel_btn = ntk_button_new("Cancel", hbox);
    ntk_box_pack_end(hbox, cancel_btn, false, false, 5);
    ntk_box_pack_end(hbox, ok_btn, false, false, 5);

    DialogState state = { .done = false, .result = NTK_DIALOG_RESULT_UNKNOWN };
    ntk_widget_connect(entry, "activated", on_entry_activated, &state);
    ntk_widget_set_user_data(ok_btn, "result", (void*)(uintptr_t)NTK_DIALOG_OK);
    ntk_widget_set_user_data(cancel_btn, "result", (void*)(uintptr_t)NTK_DIALOG_CANCEL);
    ntk_widget_connect(ok_btn, "clicked", on_button_clicked, &state);
    ntk_widget_connect(cancel_btn, "clicked", on_button_clicked, &state);

    ntk_widget_show(win);
    ntk_app_run_modal(win, &state.done);

    double val = initial_value;
    if (state.result == NTK_DIALOG_OK) {
        const char *txt = ntk_text_entry_get_text(entry);
        if (txt) {
            char *endptr;
            double parsed = strtod(txt, &endptr);
            if (endptr != txt && parsed >= min_val && parsed <= max_val) {
                val = parsed;
                if (ok) *ok = true;
            } else {
                if (ok) *ok = false;
            }
        } else {
            if (ok) *ok = false;
        }
    } else {
        if (ok) *ok = false;
    }

    ntk_window_close(win);
    return val;
}

int ntk_dialog_get_item(NtkWidget *parent, const char *title, const char *label, const char **items, int item_count, int initial_index, bool *ok) {
    NtkWidget *win = ntk_window_new_dialog(parent, title, 320, 115);
    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_box_set_spacing(vbox, 10);
    ntk_window_set_content(win, vbox);

    NtkWidget *lbl = ntk_label_new(label, vbox);
    ntk_box_pack_start(vbox, lbl, false, false, 5);

    NtkWidget *combo = ntk_combo_box_new(vbox);
    for (int i = 0; i < item_count; i++) {
        ntk_combo_box_append(combo, items[i]);
    }
    if (initial_index >= 0 && initial_index < item_count) {
        ntk_combo_box_set_selected(combo, initial_index);
    }
    ntk_box_pack_start(vbox, combo, false, false, 5);

    NtkWidget *hbox = ntk_box_new(NTK_HORIZONTAL, vbox);
    ntk_box_set_spacing(hbox, 10);
    ntk_box_pack_start(vbox, hbox, false, false, 5);

    NtkWidget *ok_btn = ntk_button_new("OK", hbox);
    NtkWidget *cancel_btn = ntk_button_new("Cancel", hbox);
    ntk_box_pack_end(hbox, cancel_btn, false, false, 5);
    ntk_box_pack_end(hbox, ok_btn, false, false, 5);

    DialogState state = { .done = false, .result = NTK_DIALOG_RESULT_UNKNOWN };
    ntk_widget_set_user_data(ok_btn, "result", (void*)(uintptr_t)NTK_DIALOG_OK);
    ntk_widget_set_user_data(cancel_btn, "result", (void*)(uintptr_t)NTK_DIALOG_CANCEL);
    ntk_widget_connect(ok_btn, "clicked", on_button_clicked, &state);
    ntk_widget_connect(cancel_btn, "clicked", on_button_clicked, &state);

    ntk_widget_show(win);
    ntk_app_run_modal(win, &state.done);

    int val = initial_index;
    if (state.result == NTK_DIALOG_OK) {
        val = ntk_combo_box_get_selected(combo);
        if (ok) *ok = true;
    } else {
        if (ok) *ok = false;
    }

    ntk_window_close(win);
    return val;
}

char* ntk_dialog_open_file(NtkWidget *parent, const char *title, const char *initial_path, const char *filter) {
    (void)filter;
    return ntk_dialog_get_text(parent, title, "Enter file path to open:", initial_path);
}

char* ntk_dialog_save_file(NtkWidget *parent, const char *title, const char *initial_path, const char *filter) {
    (void)filter;
    return ntk_dialog_get_text(parent, title, "Enter file path to save:", initial_path);
}

char* ntk_dialog_open_directory(NtkWidget *parent, const char *title, const char *initial_path) {
    return ntk_dialog_get_text(parent, title, "Enter directory path:", initial_path);
}

NtkColor ntk_dialog_pick_color(NtkWidget *parent, const char *title, NtkColor initial_color, bool *ok) {
    char init_str[32];
    snprintf(init_str, sizeof(init_str), "#%08X", initial_color);

    char *res_str = ntk_dialog_get_text(parent, title, "Enter Color Hex (e.g. #FFFFFFFF):", init_str);
    if (!res_str) {
        if (ok) *ok = false;
        return initial_color;
    }

    NtkColor val = ntk_color_from_hex(res_str);
    free(res_str);
    if (ok) *ok = true;
    return val;
}

NtkFont* ntk_dialog_pick_font(NtkWidget *parent, const char *title, NtkFont *initial_font, bool *ok) {
    NtkWidget *win = ntk_window_new_dialog(parent, title, 320, 180);
    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_box_set_spacing(vbox, 10);
    ntk_window_set_content(win, vbox);

    NtkWidget *lbl1 = ntk_label_new("Font Family:", vbox);
    ntk_box_pack_start(vbox, lbl1, false, false, 2);

    NtkWidget *family_combo = ntk_combo_box_new(vbox);
    ntk_combo_box_append(family_combo, "default");
    ntk_combo_box_append(family_combo, "fixed");
    ntk_combo_box_set_selected(family_combo, 0);
    ntk_box_pack_start(vbox, family_combo, false, false, 5);

    NtkWidget *lbl2 = ntk_label_new("Font Size:", vbox);
    ntk_box_pack_start(vbox, lbl2, false, false, 2);

    NtkWidget *size_spin = ntk_spin_box_new(vbox);
    ntk_spin_box_set_range(size_spin, 6, 72);
    ntk_spin_box_set_value(size_spin, 12);
    ntk_box_pack_start(vbox, size_spin, false, false, 5);

    NtkWidget *hbox = ntk_box_new(NTK_HORIZONTAL, vbox);
    ntk_box_set_spacing(hbox, 10);
    ntk_box_pack_start(vbox, hbox, false, false, 5);

    NtkWidget *ok_btn = ntk_button_new("OK", hbox);
    NtkWidget *cancel_btn = ntk_button_new("Cancel", hbox);
    ntk_box_pack_end(hbox, cancel_btn, false, false, 5);
    ntk_box_pack_end(hbox, ok_btn, false, false, 5);

    DialogState state = { .done = false, .result = NTK_DIALOG_RESULT_UNKNOWN };
    ntk_widget_set_user_data(ok_btn, "result", (void*)(uintptr_t)NTK_DIALOG_OK);
    ntk_widget_set_user_data(cancel_btn, "result", (void*)(uintptr_t)NTK_DIALOG_CANCEL);
    ntk_widget_connect(ok_btn, "clicked", on_button_clicked, &state);
    ntk_widget_connect(cancel_btn, "clicked", on_button_clicked, &state);

    ntk_widget_show(win);
    ntk_app_run_modal(win, &state.done);

    NtkFont *ret = initial_font;
    if (state.result == NTK_DIALOG_OK) {
        int selected_fam = ntk_combo_box_get_selected(family_combo);
        const char *family = (selected_fam == 1) ? "fixed" : "default";
        int size = ntk_spin_box_get_value(size_spin);
        ret = ntk_font_new(family, size, 0, 0); 
        if (ok) *ok = true;
    } else {
        if (ok) *ok = false;
    }

    ntk_window_close(win);
    return ret;
}
