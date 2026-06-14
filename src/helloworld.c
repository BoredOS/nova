// Copyright (c) 2023-2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

// BOREDOS_APP_DESC: A simple GUI Hello World application
// BOREDOS_APP_ICONS: /Library/images/icons/serenityicons/32x32/app-terminal.png

#include "ntk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static NtkApp *g_app_inst = NULL;

static void on_btn_clicked(NtkWidget *w, void *userdata) {
    const char *action = userdata;
    if (strcmp(action, "exit") == 0) {
        ntk_app_quit(g_app_inst);
    } else if (strcmp(action, "msg_info") == 0) {
        ntk_dialog_message(w, "Information", "This is a Nova ToolKit message dialog!", NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);
    } else if (strcmp(action, "msg_question") == 0) {
        bool answer = ntk_dialog_question(w, "Question", "Do you like Nova ToolKit?");
        if (answer) {
            ntk_dialog_message(w, "Answer", "We are glad you like it!", NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);
        } else {
            ntk_dialog_message(w, "Answer", "how dare you. i WILL find you.", NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);
        }
    } else if (strcmp(action, "get_text") == 0) {
        char *text = ntk_dialog_get_text(w, "Username", "Enter name or something, idrc:", "chris");
        if (text) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Hello, %s!", text);
            ntk_dialog_message(w, "Greeting", msg, NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);
            free(text);
        }
    } else if (strcmp(action, "click") == 0) {
        ntk_dialog_message(w, "Click", "Button was clicked!", NTK_MSG_INFO, NTK_DIALOG_BUTTONS_OK);
    }
}

static void on_canvas_draw(NtkCanvas *canvas, NtkPainter *painter, void *userdata) {
    (void)userdata;
    (void)canvas; 
    ntk_painter_clear(painter, ntk_color_from_rgb(240, 240, 240));
    ntk_painter_set_color(painter, ntk_color_from_rgb(0, 102, 204));
    ntk_painter_fill_rect(painter, NTK_RECT(20, 20, 100, 60));

    ntk_painter_set_color(painter, ntk_color_from_rgb(204, 0, 0));
    ntk_painter_draw_rounded_rect(painter, NTK_RECT(140, 20, 100, 60), 10);

    ntk_painter_set_color(painter, ntk_color_from_rgb(0, 153, 76));
    ntk_painter_fill_ellipse(painter, NTK_RECT(260, 20, 100, 60));
    ntk_painter_set_color(painter, ntk_color_from_rgb(0, 0, 0));
    ntk_painter_draw_line(painter, 20, 100, 360, 100);
    ntk_painter_draw_text(painter, "painter demo", 20, 120);
    NtkPoint start_p = NTK_POINT(20, 140);
    NtkPoint end_p = NTK_POINT(220, 200);
    NtkGradient *lg = ntk_gradient_new_linear(start_p, end_p);
    ntk_gradient_add_stop(lg, 0.0f, ntk_color_from_rgb(255, 0, 0));
    ntk_gradient_add_stop(lg, 1.0f, ntk_color_from_rgb(0, 0, 255));
    ntk_painter_draw_gradient(painter, lg, NTK_RECT(20, 140, 200, 60));
    ntk_gradient_destroy(lg);
}

int main(void) {
    g_app_inst = ntk_app_new();
    if (!g_app_inst) return 1;

    NtkWidget *win = ntk_window_new("Hello, BoredOS!", 600, 480);
    if (!win) {
        ntk_app_destroy(g_app_inst);
        return 1;
    }

    // MenuBar
    NtkWidget *menubar = ntk_menu_bar_new(win);
    NtkWidget *file_menu = ntk_menu_new();
    NtkWidget *exit_item = ntk_menu_item_new("Exit");
    ntk_widget_connect(exit_item, "clicked", on_btn_clicked, (void*)"exit");
    ntk_menu_add_item(file_menu, exit_item);
    ntk_menu_bar_add_menu(menubar, file_menu, "File");
    ntk_window_set_menubar(win, menubar);
    NtkWidget *toolbar = ntk_toolbar_new(win);
    NtkWidget *tb_btn1 = ntk_tool_button_new("Open", NULL);
    ntk_widget_connect(tb_btn1, "clicked", on_btn_clicked, (void*)"open");
    ntk_toolbar_add_button(toolbar, tb_btn1);
    ntk_window_set_toolbar(win, toolbar);
    NtkWidget *statusbar = ntk_statusbar_new(win);
    ntk_statusbar_set_text(statusbar, "Ready");
    ntk_window_set_statusbar(win, statusbar);
    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, win);
    ntk_window_set_content(win, vbox);
    NtkWidget *tabs = ntk_tab_widget_new(vbox);
    ntk_box_pack_start(vbox, tabs, true, true, 0);
    NtkWidget *basic_box = ntk_box_new(NTK_VERTICAL, tabs);
    ntk_tab_widget_add_page(tabs, basic_box, "Basic");
    NtkWidget *lbl = ntk_label_new("Welcome to Nova ToolKit!", basic_box);
    ntk_box_pack_start(basic_box, lbl, false, false, 5);

    NtkWidget *btn = ntk_button_new("Click Me!", basic_box);
    ntk_widget_connect(btn, "clicked", on_btn_clicked, (void*)"click");
    ntk_box_pack_start(basic_box, btn, false, false, 5);

    NtkWidget *chk = ntk_checkbox_new("Enable Extra Features", basic_box);
    ntk_box_pack_start(basic_box, chk, false, false, 5);

    NtkWidget *rb_box = ntk_box_new(NTK_HORIZONTAL, basic_box);
    ntk_box_pack_start(basic_box, rb_box, false, false, 5);

    NtkRadioGroup *rg = ntk_radio_group_new();
    NtkWidget *rb1 = ntk_radio_button_new("Option A", rb_box);
    NtkWidget *rb2 = ntk_radio_button_new("Option B", rb_box);
    ntk_radio_group_add(rg, rb1);
    ntk_radio_group_add(rg, rb2);
    ntk_box_pack_start(rb_box, rb1, false, false, 5);
    ntk_box_pack_start(rb_box, rb2, false, false, 5);

    NtkWidget *progress = ntk_progress_bar_new(basic_box);
    ntk_progress_bar_set_value(progress, 0.45f);
    ntk_progress_bar_set_text(progress, "45% Completed");
    ntk_box_pack_start(basic_box, progress, false, false, 5);
    NtkWidget *input_box = ntk_box_new(NTK_VERTICAL, tabs);
    ntk_tab_widget_add_page(tabs, input_box, "Input");

    NtkWidget *entry = ntk_text_entry_new(input_box);
    ntk_text_entry_set_placeholder(entry, "Enter username here...");
    ntk_box_pack_start(input_box, entry, false, false, 5);

    NtkWidget *textarea = ntk_text_area_new(input_box);
    ntk_text_area_set_text(textarea, "Nova ToolKit multi-line text area.\nSupports writing long text.");
    ntk_box_pack_start(input_box, textarea, true, true, 5);

    NtkWidget *spin = ntk_spin_box_new(input_box);
    ntk_spin_box_set_range(spin, 1, 100);
    ntk_spin_box_set_value(spin, 50);
    ntk_box_pack_start(input_box, spin, false, false, 5);

    NtkWidget *slider = ntk_slider_new(NTK_HORIZONTAL, input_box);
    ntk_slider_set_range(slider, 0, 100);
    ntk_slider_set_value(slider, 20);
    ntk_box_pack_start(input_box, slider, false, false, 5);

    NtkWidget *combo = ntk_combo_box_new(input_box);
    ntk_combo_box_append(combo, "Low Resolution");
    ntk_combo_box_append(combo, "Medium Resolution");
    ntk_combo_box_append(combo, "High Resolution");
    ntk_combo_box_set_selected(combo, 1);
    ntk_box_pack_start(input_box, combo, false, false, 5);

    // Tab 3: Collections
    NtkWidget *coll_box = ntk_box_new(NTK_VERTICAL, tabs);
    ntk_tab_widget_add_page(tabs, coll_box, "Collections");

    NtkWidget *listbox = ntk_list_box_new(coll_box);
    ntk_list_box_append(listbox, "List Item #1");
    ntk_list_box_append(listbox, "List Item #2");
    ntk_list_box_append(listbox, "List Item #3");
    ntk_box_pack_start(coll_box, listbox, false, false, 5);

    NtkWidget *treeview = ntk_tree_view_new(coll_box);
    NtkTreeNode *t_root = ntk_tree_view_get_root(treeview);
    NtkTreeNode *node1 = ntk_tree_node_add_child(t_root, "Workplace");
    ntk_tree_node_add_child(node1, "Project Files");
    ntk_tree_node_add_child(node1, "Design Mockups");
    ntk_box_pack_start(coll_box, treeview, false, false, 5);

    NtkWidget *tableview = ntk_table_view_new(coll_box);
    const char *tbl_headers[] = {"ID", "Name", "Role"};
    int tbl_widths[] = {40, 120, 120};
    ntk_table_view_set_columns(tableview, 3, tbl_headers, tbl_widths);
    ntk_table_view_set_row_count(tableview, 2);
    ntk_table_view_set_cell(tableview, 0, 0, "1");
    ntk_table_view_set_cell(tableview, 0, 1, "Christiaan");
    ntk_table_view_set_cell(tableview, 0, 2, "the goat");
    ntk_table_view_set_cell(tableview, 1, 0, "2");
    ntk_table_view_set_cell(tableview, 1, 1, "ntk");
    ntk_table_view_set_cell(tableview, 1, 2, "very tuff toolkit");
    ntk_box_pack_start(coll_box, tableview, false, false, 5);
    NtkWidget *lay_box = ntk_box_new(NTK_VERTICAL, tabs);
    ntk_tab_widget_add_page(tabs, lay_box, "Layouts");

    NtkWidget *splitter = ntk_splitter_new(NTK_HORIZONTAL, lay_box);
    NtkWidget *sp_left = ntk_box_new(NTK_VERTICAL, splitter);
    NtkWidget *sp_left_lbl = ntk_label_new("Volume Setting:", sp_left);
    ntk_box_pack_start(sp_left, sp_left_lbl, false, false, 2);
    NtkWidget *sp_left_slider = ntk_slider_new(NTK_HORIZONTAL, sp_left);
    ntk_box_pack_start(sp_left, sp_left_slider, false, false, 2);

    NtkWidget *sp_right = ntk_box_new(NTK_VERTICAL, splitter);
    NtkWidget *sp_right_lbl = ntk_label_new("User Name Input:", sp_right);
    ntk_box_pack_start(sp_right, sp_right_lbl, false, false, 2);
    NtkWidget *sp_right_entry = ntk_text_entry_new(sp_right);
    ntk_box_pack_start(sp_right, sp_right_entry, false, false, 2);

    ntk_splitter_set_widgets(splitter, sp_left, sp_right);
    ntk_box_pack_start(lay_box, splitter, false, false, 5);

    NtkWidget *groupbox = ntk_group_box_new("Group Settings", lay_box);
    NtkWidget *group_vbox = ntk_box_new(NTK_VERTICAL, groupbox);
    NtkWidget *group_lbl = ntk_label_new("Configure group attributes below:", group_vbox);
    ntk_box_pack_start(group_vbox, group_lbl, false, false, 2);

    NtkWidget *group_hbox = ntk_box_new(NTK_HORIZONTAL, group_vbox);
    NtkWidget *grp_check = ntk_checkbox_new("Enable Feature Alpha", group_hbox);
    ntk_box_pack_start(group_hbox, grp_check, false, false, 5);
    NtkWidget *grp_spin = ntk_spin_box_new(group_hbox);
    ntk_box_pack_start(group_hbox, grp_spin, false, false, 5);
    ntk_box_pack_start(group_vbox, group_hbox, false, false, 2);

    ntk_box_pack_start(lay_box, groupbox, false, false, 5);

    NtkWidget *grid = ntk_grid_new(lay_box);
    NtkWidget *grid_chk1 = ntk_checkbox_new("Grid Option A", grid);
    NtkWidget *grid_btn1 = ntk_button_new("Grid Action X", grid);
    NtkWidget *grid_lbl2 = ntk_label_new("Grid Status Info", grid);
    NtkWidget *grid_chk2 = ntk_checkbox_new("Grid Option B", grid);

    ntk_grid_attach(grid, grid_chk1, 0, 0, 1, 1);
    ntk_grid_attach(grid, grid_btn1, 0, 1, 1, 1);
    ntk_grid_attach(grid, grid_lbl2, 1, 0, 1, 1);
    ntk_grid_attach(grid, grid_chk2, 1, 1, 1, 1);

    ntk_box_pack_start(lay_box, grid, false, false, 5);

    NtkWidget *draw_box = ntk_box_new(NTK_VERTICAL, tabs);
    ntk_tab_widget_add_page(tabs, draw_box, "Drawing");

    NtkWidget *canvas = ntk_canvas_new(draw_box);
    ntk_canvas_set_draw_callback(canvas, on_canvas_draw, NULL);
    ntk_box_pack_start(draw_box, canvas, true, true, 5);
    NtkWidget *dialogs_box = ntk_box_new(NTK_VERTICAL, tabs);
    ntk_tab_widget_add_page(tabs, dialogs_box, "Dialogs");

    NtkWidget *dlg_btn1 = ntk_button_new("Info Message Box", dialogs_box);
    ntk_widget_connect(dlg_btn1, "clicked", on_btn_clicked, (void*)"msg_info");
    ntk_box_pack_start(dialogs_box, dlg_btn1, false, false, 5);

    NtkWidget *dlg_btn2 = ntk_button_new("Question Box", dialogs_box);
    ntk_widget_connect(dlg_btn2, "clicked", on_btn_clicked, (void*)"msg_question");
    ntk_box_pack_start(dialogs_box, dlg_btn2, false, false, 5);

    NtkWidget *dlg_btn3 = ntk_button_new("Get Text Input", dialogs_box);
    ntk_widget_connect(dlg_btn3, "clicked", on_btn_clicked, (void*)"get_text");
    ntk_box_pack_start(dialogs_box, dlg_btn3, false, false, 5);
    ntk_widget_show(win);
    int rc = ntk_app_run(g_app_inst);

    ntk_radio_group_destroy(rg);
    ntk_app_destroy(g_app_inst);
    return rc;
}