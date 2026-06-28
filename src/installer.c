// Copyright (c) 2026 Christiaan (chris@boreddev.nl)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <syscall.h>

#define MAX_DISKS 16
#define MAX_OPTIONS 64

typedef struct {
    char devname[16];
    uint32_t sectors;
    uint32_t mb;
} TargetDisk;

typedef struct {
    char filename[64];
    char pkgname[64];
    int enabled;
    NtkWidget *chk_widget;
} PackageOption;
static NtkApp *g_app = NULL;
static NtkWidget *g_win = NULL;
static NtkWidget *g_stack = NULL;
static TargetDisk g_disks[MAX_DISKS];
static int g_num_disks = 0;
static int g_selected_disk_idx = -1;
static PackageOption g_options[MAX_OPTIONS];
static int g_num_options = 0;
static char g_excludes[512][128];
static int g_num_excludes = 0;
static NtkWidget *g_btn_back = NULL;
static NtkWidget *g_btn_next = NULL;
static NtkWidget *g_btn_cancel = NULL;
static NtkWidget *g_chk_warning_accept = NULL;
static NtkWidget *g_progress_bar = NULL;
static NtkWidget *g_lbl_progress_status = NULL;
static NtkWidget *g_lbl_confirm_disk = NULL;
static NtkWidget *g_lbl_confirm_pkgs = NULL;
#define PAGE_WELCOME  "welcome"
#define PAGE_DISK     "disk"
#define PAGE_TYPE     "type"
#define PAGE_PACKAGES "packages"
#define PAGE_WARNING  "warning"
#define PAGE_CONFIRM  "confirm"
#define PAGE_PROGRESS "progress"
#define PAGE_FINISH   "finish"

static const char* g_pages[] = {
    PAGE_WELCOME,
    PAGE_DISK,
    PAGE_TYPE,
    PAGE_PACKAGES,
    PAGE_WARNING,
    PAGE_CONFIRM,
    PAGE_PROGRESS,
    PAGE_FINISH
};
static int g_current_page_idx = 0;

typedef enum {
    INSTALL_TYPE_TYPICAL = 0,
    INSTALL_TYPE_CUSTOM
} InstallType;

static InstallType g_install_type = INSTALL_TYPE_TYPICAL;
static NtkRadioGroup *g_type_radio_group = NULL;

typedef enum {
    INSTALL_STATE_INIT = 0,
    INSTALL_STATE_PARTITION,
    INSTALL_STATE_PARTITION_WAIT,
    INSTALL_STATE_FORMAT,
    INSTALL_STATE_MOUNT,
    INSTALL_STATE_COPY_BIN,
    INSTALL_STATE_COPY_LIB,
    INSTALL_STATE_COPY_OTHER,
    INSTALL_STATE_COPY_PACKAGES,
    INSTALL_STATE_COPY_PACKAGES_WAIT,
    INSTALL_STATE_BOOTLOADER,
    INSTALL_STATE_FINALIZING,
    INSTALL_STATE_DONE,
    INSTALL_STATE_ERROR
} InstallState;

static int g_fdisk_pid = -1;
static int g_bpm_pid = -1;

static int g_is_uefi = 1;
static int g_esp_size_mb = 512;

static char g_esp_dev[16] = {0};
static char g_root_dev[16] = {0};

static int g_pkg_install_index = 0;
static int g_pkg_total_checked = 0;

static char g_error_message[256] = {0};

static int sc_strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static int sc_strncpy(char *dst, const char *src, int n) {
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
    return i;
}

static int get_available_disks(TargetDisk *disks, int max_disks) {
    int count = 0;
    int n = sys_disk_get_count();
    for (int i = 0; i < n; i++) {
        disk_info_t d;
        if (sys_disk_get_info(i, &d) != 0) continue;
        if (!d.is_partition && d.total_sectors > 0) {
            sc_strncpy(disks[count].devname, d.devname, sizeof(disks[count].devname));
            disks[count].sectors = d.total_sectors;
            disks[count].mb = d.total_sectors / 2048;
            count++;
            if (count >= max_disks) break;
        }
    }
    return count;
}

static int get_package_options(PackageOption *options, int max_options) {
    int count = 0;
    FAT32_FileInfo *entries = (FAT32_FileInfo *)malloc(sizeof(FAT32_FileInfo) * 32);
    if (!entries) return 0;
    int offset = 0;
    while (count < max_options) {
        int n = sys_list_offset("/usr/share/packages", entries, 32, offset);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            if (entries[i].is_directory) continue;
            size_t len = strlen(entries[i].name);
            if (len > 4 && strcmp(entries[i].name + len - 4, ".bup") == 0) {
                sc_strncpy(options[count].filename, entries[i].name, sizeof(options[count].filename));
                sc_strncpy(options[count].pkgname, entries[i].name, sizeof(options[count].pkgname));
                options[count].pkgname[len - 4] = '\0';
                options[count].enabled = 1;
                count++;
                if (count >= max_options) break;
            }
        }
        offset += n;
    }
    free(entries);
    return count;
}

static void load_excludes(void) {
    int fd = sys_open("/usr/share/packages/excludes.txt", "r");
    if (fd < 0) return;
    
    char *buf = (char*)malloc(32768);
    if (!buf) { sys_close(fd); return; }
    
    int n = sys_read(fd, buf, 32768 - 1);
    if (n > 0) {
        buf[n] = '\0';
        char *line = buf;
        while (line && *line && g_num_excludes < 512) {
            char *next = strchr(line, '\n');
            if (next) *next = '\0';
            
            int len = strlen(line);
            if (len > 0 && line[len - 1] == '\r') line[len - 1] = '\0';
            
            if (line[0] != '\0') {
                sc_strncpy(g_excludes[g_num_excludes], line, 128);
                g_num_excludes++;
            }
            
            if (next) line = next + 1;
            else line = NULL;
        }
    }
    free(buf);
    sys_close(fd);
}

static int should_exclude(const char *path) {
    if (sc_strcmp(path, "/bin/boredos_install") == 0 ||
        sc_strcmp(path, "/bin/boredos_install.elf") == 0 ||
        sc_strcmp(path, "/bin/installer") == 0 ||
        sc_strcmp(path, "/bin/installer.elf") == 0 ||
        sc_strcmp(path, "/usr/share/applications/installer.desktop") == 0) {
        return 1;
    }
    for (int i = 0; i < g_num_excludes; i++) {
        if (sc_strcmp(path, g_excludes[i]) == 0) {
            return 1;
        }
        int len = strlen(g_excludes[i]);
        if (strncmp(path, g_excludes[i], len) == 0 && (path[len] == '/' || path[len] == '\0')) {
            return 1;
        }
    }
    return 0;
}

static int copy_file(const char *src, const char *dst) {
    int sfd = sys_open(src, "r");
    if (sfd < 0) return -1;
    sys_delete(dst);
    int dfd = sys_open(dst, "w");
    if (dfd < 0) { sys_close(sfd); return -1; }

    char *buf = (char*)malloc(65536);
    if (!buf) { sys_close(sfd); sys_close(dfd); return -1; }
    int n;
    while ((n = sys_read(sfd, buf, 65536)) > 0) {
        if (sys_write_fs(dfd, buf, n) != n) {
            sys_close(sfd); sys_close(dfd);
            free(buf);
            return -1;
        }
    }
    free(buf);
    sys_close(sfd);
    sys_close(dfd);
    return 0;
}

static int copy_file_optional(const char *src, const char *dst) {
    if (!sys_exists(src)) return 0;
    return copy_file(src, dst);
}

static int copy_tree(const char *src_dir, const char *dst_dir) {
    if (should_exclude(src_dir)) return 0;
    sys_mkdir(dst_dir);
    
    int chunk_size = 128;
    FAT32_FileInfo *entries = (FAT32_FileInfo *)malloc(sizeof(FAT32_FileInfo) * chunk_size);
    if (!entries) return -1;
    
    int offset = 0;
    while (1) {
        int n = sys_list_offset(src_dir, entries, chunk_size, offset);
        if (n <= 0) break;
        for (int i = 0; i < n; i++) {
            if (entries[i].name[0] == '.' && entries[i].name[1] == '_') continue;

            char src_path[512], dst_path[512];
            snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entries[i].name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entries[i].name);

            if (should_exclude(src_path)) continue;

            if (entries[i].is_directory) {
                if (copy_tree(src_path, dst_path) != 0) {
                    free(entries);
                    return -1;
                }
            } else {
                if (copy_file(src_path, dst_path) != 0) {
                    free(entries);
                    return -1;
                }
            }
        }
        offset += n;
    }
    free(entries);
    return 0;
}

static void set_progress(float pct, const char *status) {
    ntk_progress_bar_set_value(g_progress_bar, pct);
    char buf[64];
    snprintf(buf, sizeof(buf), "%d%% Completed", (int)(pct * 100));
    ntk_progress_bar_set_text(g_progress_bar, buf);
    ntk_label_set_text(g_lbl_progress_status, status);
    ntk_app_draw_windows();
}

static void update_page_state(void) {
    if (g_current_page_idx == 0) {
        ntk_widget_set_enabled(g_btn_back, false);
        ntk_widget_set_enabled(g_btn_next, true);
        ntk_button_set_label(g_btn_next, "Next >");
        ntk_widget_set_enabled(g_btn_cancel, true);
    } else if (g_current_page_idx == 1 && g_num_disks == 0) {
        ntk_widget_set_enabled(g_btn_back, true);
        ntk_widget_set_enabled(g_btn_next, false);
        ntk_button_set_label(g_btn_next, "Next >");
        ntk_widget_set_enabled(g_btn_cancel, true);
    } else if (g_current_page_idx == 4) {
        ntk_widget_set_enabled(g_btn_back, true);
        bool accepted = ntk_checkbox_is_checked(g_chk_warning_accept);
        ntk_widget_set_enabled(g_btn_next, accepted);
        ntk_button_set_label(g_btn_next, "Next >");
        ntk_widget_set_enabled(g_btn_cancel, true);
    } else if (g_current_page_idx == 5) {
        ntk_widget_set_enabled(g_btn_back, true);
        ntk_widget_set_enabled(g_btn_next, true);
        ntk_button_set_label(g_btn_next, "Install");
        ntk_widget_set_enabled(g_btn_cancel, true);
    } else if (g_current_page_idx == 6) {
        ntk_widget_set_enabled(g_btn_back, false);
        ntk_widget_set_enabled(g_btn_next, false);
        ntk_widget_set_enabled(g_btn_cancel, false);
    } else if (g_current_page_idx == 7) {
        ntk_widget_set_enabled(g_btn_back, false);
        ntk_widget_set_enabled(g_btn_next, true);
        ntk_button_set_label(g_btn_next, "Finish");
        ntk_widget_set_enabled(g_btn_cancel, false);
    } else {
        ntk_widget_set_enabled(g_btn_back, true);
        ntk_widget_set_enabled(g_btn_next, true);
        ntk_button_set_label(g_btn_next, "Next >");
        ntk_widget_set_enabled(g_btn_cancel, true);
    }
}

static void on_sidebar_draw(NtkCanvas *canvas, NtkPainter *painter, void *userdata) {
    (void)canvas;
    (void)userdata;
    NtkWidget *widget = (NtkWidget *)canvas;
    NtkSize sz = ntk_widget_get_size(widget);
    NtkPoint origin = ntk_widget_map_to_global(widget, NTK_POINT(0, 0));
    NtkRect rect = NTK_RECT(origin.x, origin.y, sz.width, sz.height);
    
    NtkPoint start_p = NTK_POINT(origin.x, origin.y);
    NtkPoint end_p = NTK_POINT(origin.x, origin.y + sz.height);
    
    NtkGradient *g = ntk_gradient_new_linear(start_p, end_p);
    ntk_gradient_add_stop(g, 0.0f, ntk_color_from_rgba(20, 50, 110, 255));
    ntk_gradient_add_stop(g, 0.5f, ntk_color_from_rgba(40, 20, 90, 255));
    ntk_gradient_add_stop(g, 1.0f, ntk_color_from_rgba(15, 10, 35, 255));
    
    ntk_painter_draw_gradient(painter, g, rect);
    ntk_gradient_destroy(g);
    
    ntk_painter_set_color(painter, ntk_color_from_rgba(255, 255, 255, 12));
    ntk_painter_fill_ellipse(painter, NTK_RECT(origin.x - 30, origin.y + sz.height - 110, 180, 180));
    ntk_painter_fill_ellipse(painter, NTK_RECT(origin.x + sz.width - 50, origin.y - 40, 100, 100));
    
    NtkStyle *style = ntk_widget_get_style(widget);
    NtkFont *font = ntk_style_get_font(style, NTK_STYLE_ELEMENT_DEFAULT_FONT);
    ntk_painter_set_font(painter, font);
    ntk_painter_set_color(painter, ntk_color_from_rgba(255, 255, 255, 255));
    ntk_painter_draw_text(painter, "Install", origin.x + 16, origin.y + 30);
    ntk_painter_draw_text(painter, "Assistant", origin.x + 16, origin.y + 50);
}

static void on_separator_draw(NtkCanvas *canvas, NtkPainter *painter, void *userdata) {
    (void)userdata;
    NtkWidget *widget = (NtkWidget *)canvas;
    NtkSize sz = ntk_widget_get_size(widget);
    NtkPoint origin = ntk_widget_map_to_global(widget, NTK_POINT(0, 0));
    NtkStyle *style = ntk_widget_get_style(widget);
    NtkColor dark = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_DARK);
    NtkColor light = ntk_style_get_color(style, NTK_STYLE_ROLE_BORDER_LIGHT);
    
    ntk_painter_set_color(painter, dark);
    ntk_painter_draw_line(painter, origin.x, origin.y, origin.x + sz.width, origin.y);
    ntk_painter_set_color(painter, light);
    ntk_painter_draw_line(painter, origin.x, origin.y + 1, origin.x + sz.width, origin.y + 1);
}

static void run_installation_sync(void) {
    int status = 0;
    const char *devname = g_disks[g_selected_disk_idx].devname;

    set_progress(0.05f, "Partitioning target disk...");
    
    char fdisk_args[128];
    int ai = 0;
    const char *cmd = "--script ";
    for (; *cmd; cmd++) fdisk_args[ai++] = *cmd;
    if (g_is_uefi) {
        const char *u = "--uefi --esp-size ";
        for (; *u; u++) fdisk_args[ai++] = *u;
        char num[16]; int ni = 0;
        int v = g_esp_size_mb;
        if (v == 0) { num[ni++] = '0'; } else {
            char tmp[16]; int ti = 0;
            while (v > 0) { tmp[ti++] = '0' + (v % 10); v /= 10; }
            for (int j = ti - 1; j >= 0; j--) num[ni++] = tmp[j];
        }
        for (int j = 0; j < ni; j++) fdisk_args[ai++] = num[j];
        fdisk_args[ai++] = ' ';
    } else {
        const char *b = "--mbr "; for (; *b; b++) fdisk_args[ai++] = *b;
    }
    fdisk_args[ai++] = '/'; fdisk_args[ai++] = 'd'; fdisk_args[ai++] = 'e';
    fdisk_args[ai++] = 'v'; fdisk_args[ai++] = '/';
    for (int j = 0; devname[j]; j++) fdisk_args[ai++] = devname[j];
    fdisk_args[ai] = 0;

    g_fdisk_pid = sys_spawn("/bin/fdisk.elf", fdisk_args, 0, 0);
    if (g_fdisk_pid < 0) {
        ntk_dialog_error(g_win, "Installation Error", "Failed to spawn partitioning tool.");
        goto error;
    }
    
    waitpid(g_fdisk_pid, &status, 0);
    if (status != 0) {
        ntk_dialog_error(g_win, "Installation Error", "fdisk failed to partition the disk.");
        goto error;
    }
    
    sys_disk_rescan(devname);
    
    g_esp_dev[0] = '\0';
    g_root_dev[0] = '\0';
    int n = sys_disk_get_count();
    for (int i = 0; i < n; i++) {
        disk_info_t d;
        if (sys_disk_get_info(i, &d) != 0) continue;
        if (!d.is_partition) continue;
        int match = 1;
        for (int j = 0; devname[j]; j++) {
            if (d.devname[j] != devname[j]) { match = 0; break; }
        }
        if (!match) continue;
        if (g_is_uefi && d.is_esp && !g_esp_dev[0])
            sc_strncpy(g_esp_dev, d.devname, 16);
        else if (!d.is_esp && !g_root_dev[0]) {
            sc_strncpy(g_root_dev, d.devname, 16);
        }
    }
    
    if (!g_root_dev[0] || (g_is_uefi && !g_esp_dev[0])) {
        ntk_dialog_error(g_win, "Installation Error", "Could not locate partitions post-fdisk.");
        goto error;
    }

    set_progress(0.15f, "Formatting partitions...");
    if (g_is_uefi) {
        if (sys_disk_mkfs_fat32(g_esp_dev, "EFI") != 0) {
            ntk_dialog_error(g_win, "Installation Error", "Failed to format the ESP partition.");
            goto error;
        }
    }
    if (sys_disk_mkfs_fat32(g_root_dev, "BOREDOS") != 0) {
        ntk_dialog_error(g_win, "Installation Error", "Failed to format the root partition.");
        goto error;
    }

    set_progress(0.20f, "Mounting target partitions...");
    sys_mkdir("/mnt");
    sys_mkdir("/mnt/boot");
    sys_mkdir("/mnt/esp");
    
    if (sys_disk_mount(g_root_dev, "/mnt") != 0) {
        ntk_dialog_error(g_win, "Installation Error", "Failed to mount root partition.");
        goto error;
    }
    if (g_is_uefi) {
        sys_mkdir("/mnt/boot");
        if (sys_disk_mount(g_esp_dev, "/mnt/boot") != 0) {
            ntk_dialog_error(g_win, "Installation Error", "Failed to mount ESP partition.");
            goto error;
        }
    } else {
        sys_mkdir("/mnt/boot");
    }
    
    set_progress(0.30f, "Copying system binaries (/bin)...");
    if (copy_tree("/bin", "/mnt/bin") != 0) {
        ntk_dialog_error(g_win, "Installation Error", "Failed to copy system binaries.");
        goto error;
    }

    set_progress(0.45f, "Copying system libraries (/Library)...");
    if (copy_tree("/Library", "/mnt/Library") != 0) {
        ntk_dialog_error(g_win, "Installation Error", "Failed to copy system libraries.");
        goto error;
    }

    set_progress(0.60f, "Copying other essential system paths...");
    copy_tree("/docs", "/mnt/docs");
    copy_tree("/root", "/mnt/root");
    copy_tree("/usr", "/mnt/usr");
    copy_tree("/etc", "/mnt/etc");
    
    if (copy_file("/boot/boredos.elf", "/mnt/boot/boredos.elf") != 0) {
        ntk_dialog_error(g_win, "Installation Error", "Failed to copy kernel.");
        goto error;
    }

    g_pkg_install_index = 0;
    g_pkg_total_checked = 0;
    for (int i = 0; i < g_num_options; i++) {
        if (g_options[i].enabled) g_pkg_total_checked++;
    }

    for (int i = 0; i < g_num_options; i++) {
        if (g_options[i].enabled) {
            float pct = 0.70f + (g_pkg_install_index * 0.20f) / g_pkg_total_checked;
            char msg[128];
            snprintf(msg, sizeof(msg), "Installing package %s...", g_options[i].pkgname);
            set_progress(pct, msg);
            
            char bpm_args[256];
            snprintf(bpm_args, sizeof(bpm_args), "--root /mnt install /usr/share/packages/%s", g_options[i].filename);
            
            g_bpm_pid = sys_spawn("/bin/bpm.elf", bpm_args, 0, 0);
            if (g_bpm_pid >= 0) {
                waitpid(g_bpm_pid, &status, 0);
            }
            g_pkg_install_index++;
        }
    }

    set_progress(0.95f, "Configuring Limine bootloader...");
    if (g_is_uefi) {
        sys_mkdir("/mnt/boot/EFI");
        sys_mkdir("/mnt/boot/EFI/BOOT");
        copy_file("/boot/BOOTX64.EFI", "/mnt/boot/EFI/BOOT/BOOTX64.EFI");
        copy_file_optional("/boot/BOOTIA32.EFI", "/mnt/boot/EFI/BOOT/BOOTIA32.EFI");
        
        int fd = sys_open("/mnt/boot/limine.conf", "w");
        if (fd >= 0) {
            char cfg[512];
            int len = snprintf(cfg, sizeof(cfg),
                "timeout: 3\n"
                "verbose: yes\n"
                "\n"
                "/BoredOS\n"
                "    protocol: limine\n"
                "    path: boot():/boredos.elf\n"
                "    cmdline: -v root=/dev/%s --disk\n",
                g_root_dev);
            if (len > 0) sys_write_fs(fd, cfg, len);
            sys_close(fd);
        }
    } else {
        copy_file_optional("/boot/limine-bios.sys", "/mnt/limine-bios.sys");
        int fd = sys_open("/mnt/limine.conf", "w");
        if (fd >= 0) {
            char cfg[512];
            int len = snprintf(cfg, sizeof(cfg),
                "timeout: 3\n"
                "verbose: yes\n"
                "\n"
                "/BoredOS\n"
                "    protocol: limine\n"
                "    root: boot()\n"
                "    path: /boredos.elf\n"
                "    cmdline: -v root=/dev/%s --disk\n",
                g_root_dev);
            if (len > 0) sys_write_fs(fd, cfg, len);
            sys_close(fd);
        }
    }

    set_progress(0.98f, "Finalizing installation (syncing files)...");
    if (g_is_uefi) {
        sys_disk_sync("/mnt/boot");
        sys_disk_umount("/mnt/boot");
    }
    sys_disk_sync("/mnt");
    sys_disk_umount("/mnt");
    sys_disk_sync(devname);

    set_progress(1.00f, "Installation complete!");
    g_current_page_idx = 7;
    ntk_stack_set_visible_page(g_stack, PAGE_FINISH);
    update_page_state();
    return;

error:
    g_current_page_idx = 5; 
    ntk_stack_set_visible_page(g_stack, PAGE_CONFIRM);
    update_page_state();
}

static void on_btn_cancel_clicked(NtkWidget *w, void *userdata) {
    (void)w; (void)userdata;
    bool yes = ntk_dialog_question(g_win, "Exit Setup", "Are you sure you want to cancel the installation?");
    if (yes) {
        ntk_app_quit(g_app);
    }
}

static void on_btn_back_clicked(NtkWidget *w, void *userdata) {
    (void)w; (void)userdata;
    if (g_current_page_idx > 0) {
        if (g_current_page_idx == 4 && g_install_type == INSTALL_TYPE_TYPICAL) {
            g_current_page_idx = 2;
        } else {
            g_current_page_idx--;
        }
        ntk_stack_set_visible_page(g_stack, g_pages[g_current_page_idx]);
        update_page_state();
    }
}

static NtkWidget *g_disk_listbox = NULL;

static void on_btn_next_clicked(NtkWidget *w, void *userdata) {
    (void)w; (void)userdata;
    
    if (g_current_page_idx == 1) {
        g_selected_disk_idx = ntk_list_box_get_selected(g_disk_listbox);
        if (g_selected_disk_idx < 0 || g_selected_disk_idx >= g_num_disks) {
            ntk_dialog_warning(g_win, "No Disk Selected", "Please select a target disk to proceed.");
            return;
        }
    } else if (g_current_page_idx == 2) {
        int selected_idx = ntk_radio_group_get_selected(g_type_radio_group);
        if (selected_idx == 0) {
            g_install_type = INSTALL_TYPE_TYPICAL;
            for (int i = 0; i < g_num_options; i++) {
                g_options[i].enabled = 1;
                if (g_options[i].chk_widget) {
                    ntk_checkbox_set_checked(g_options[i].chk_widget, true);
                }
            }
            g_current_page_idx = 4;
            ntk_stack_set_visible_page(g_stack, PAGE_WARNING);
            update_page_state();
            return;
        } else {
            g_install_type = INSTALL_TYPE_CUSTOM;
        }
    } else if (g_current_page_idx == 3) {
        for (int i = 0; i < g_num_options; i++) {
            if (g_options[i].chk_widget) {
                g_options[i].enabled = ntk_checkbox_is_checked(g_options[i].chk_widget);
            }
        }
    } else if (g_current_page_idx == 4) {
        char disk_buf[128];
        snprintf(disk_buf, sizeof(disk_buf), "Target Disk: /dev/%s (%u MB)", 
                 g_disks[g_selected_disk_idx].devname, g_disks[g_selected_disk_idx].mb);
        ntk_label_set_text(g_lbl_confirm_disk, disk_buf);
        
        char pkgs_buf[256] = "";
        int first = 1;
        for (int i = 0; i < g_num_options; i++) {
            if (g_options[i].enabled) {
                if (!first) strncat(pkgs_buf, ", ", sizeof(pkgs_buf) - strlen(pkgs_buf) - 1);
                strncat(pkgs_buf, g_options[i].pkgname, sizeof(pkgs_buf) - strlen(pkgs_buf) - 1);
                first = 0;
            }
        }
        if (first) {
            strcpy(pkgs_buf, "None");
        }
        ntk_label_set_text(g_lbl_confirm_pkgs, pkgs_buf);
    } else if (g_current_page_idx == 5) {
        bool proceed = ntk_dialog_question(g_win, "Confirm Installation", "All partitions on the selected disk will be erased. Proceed?");
        if (!proceed) return;
        
        g_current_page_idx = 6;
        ntk_stack_set_visible_page(g_stack, PAGE_PROGRESS);
        update_page_state();
        
        run_installation_sync();
        return;
    } else if (g_current_page_idx == 7) {
        sys_reboot();
        ntk_app_quit(g_app);
        return;
    }
    
    if (g_current_page_idx + 1 < (int)(sizeof(g_pages)/sizeof(g_pages[0]))) {
        g_current_page_idx++;
        ntk_stack_set_visible_page(g_stack, g_pages[g_current_page_idx]);
        update_page_state();
    }
}

static void on_warning_accept_changed(NtkWidget *w, void *userdata) {
    (void)userdata;
    bool accepted = ntk_checkbox_is_checked(w);
    ntk_widget_set_enabled(g_btn_next, accepted);
}

int main(void) {
    g_app = ntk_app_new();
    if (!g_app) return 1;
    
    load_excludes();
    g_num_disks = get_available_disks(g_disks, MAX_DISKS);
    g_num_options = get_package_options(g_options, MAX_OPTIONS);
    
    g_win = ntk_window_new("BoredOS Install Assistant", 640, 420);
    if (!g_win) {
        ntk_app_destroy(g_app);
        return 1;
    }
    ntk_window_set_icon_path(g_win, "/Library/Icons/serenityicons/16x16/app-assistant.png");
    ntk_window_set_resizable(g_win, false);
    
    NtkWidget *hbox_main = ntk_box_new(NTK_HORIZONTAL, g_win);
    ntk_window_set_content(g_win, hbox_main);
    
    NtkWidget *canvas_sidebar = ntk_canvas_new(hbox_main);
    ntk_widget_set_min_size(canvas_sidebar, NTK_SIZE(160, 420));
    ntk_canvas_set_draw_callback(canvas_sidebar, on_sidebar_draw, NULL);
    ntk_box_pack_start(hbox_main, canvas_sidebar, false, true, 0);
    
    NtkWidget *vbox_right = ntk_box_new(NTK_VERTICAL, hbox_main);
    ntk_box_set_spacing(vbox_right, 8);
    ntk_box_pack_start(hbox_main, vbox_right, true, true, 0);
    
    g_stack = ntk_stack_new(vbox_right);
    ntk_widget_set_margin(g_stack, NTK_INSETS(12, 16, 12, 16));
    ntk_box_pack_start(vbox_right, g_stack, true, true, 0);
    
    NtkWidget *p_welcome = ntk_box_new(NTK_VERTICAL, g_stack);
    ntk_box_set_spacing(p_welcome, 12);
    ntk_stack_add_page(g_stack, p_welcome, PAGE_WELCOME);
    
    NtkWidget *lbl_welcome_title = ntk_label_new("Welcome to the BoredOS Install Assistant", p_welcome);
    ntk_box_pack_start(p_welcome, lbl_welcome_title, false, false, 5);
    
    NtkWidget *lbl_welcome_desc1 = ntk_label_new("This assistant will install BoredOS on your computer.", p_welcome);
    ntk_box_pack_start(p_welcome, lbl_welcome_desc1, false, false, 2);
    
    NtkWidget *lbl_welcome_desc2 = ntk_label_new("It is strongly recommended that you backup all data on the target drive before continuing.", p_welcome);
    ntk_box_pack_start(p_welcome, lbl_welcome_desc2, false, false, 2);
    
    NtkWidget *lbl_welcome_desc3 = ntk_label_new("Click Next to proceed with selecting the target installation drive.", p_welcome);
    ntk_box_pack_start(p_welcome, lbl_welcome_desc3, false, false, 2);
    
    NtkWidget *p_disk = ntk_box_new(NTK_VERTICAL, g_stack);
    ntk_box_set_spacing(p_disk, 8);
    ntk_stack_add_page(g_stack, p_disk, PAGE_DISK);
    
    NtkWidget *lbl_disk_title = ntk_label_new("Select Target Disk", p_disk);
    ntk_box_pack_start(p_disk, lbl_disk_title, false, false, 5);
    
    NtkWidget *lbl_disk_desc = ntk_label_new("Choose the disk to install BoredOS to:", p_disk);
    ntk_box_pack_start(p_disk, lbl_disk_desc, false, false, 2);
    
    if (g_num_disks == 0) {
        NtkWidget *lbl_no_disks = ntk_label_new("No hard disks detected on this computer.\nCannot install BoredOS.", p_disk);
        ntk_label_set_color(lbl_no_disks, ntk_color_from_rgba(255, 30, 30, 255));
        ntk_box_pack_start(p_disk, lbl_no_disks, false, false, 15);
    } else {
        g_disk_listbox = ntk_list_box_new(p_disk);
        for (int i = 0; i < g_num_disks; i++) {
            char buf[128];
            snprintf(buf, sizeof(buf), "/dev/%s  -  %u MB", g_disks[i].devname, g_disks[i].mb);
            ntk_list_box_append(g_disk_listbox, buf);
        }
        ntk_list_box_set_selected(g_disk_listbox, 0);
        ntk_box_pack_start(p_disk, g_disk_listbox, true, true, 2);
    }
    
    NtkWidget *p_type = ntk_box_new(NTK_VERTICAL, g_stack);
    ntk_box_set_spacing(p_type, 10);
    ntk_stack_add_page(g_stack, p_type, PAGE_TYPE);

    NtkWidget *lbl_type_title = ntk_label_new("Choose Setup Type", p_type);
    ntk_box_pack_start(p_type, lbl_type_title, false, false, 5);

    NtkWidget *lbl_type_desc = ntk_label_new("Select the setup type that best suits your needs:", p_type);
    ntk_box_pack_start(p_type, lbl_type_desc, false, false, 2);

    NtkWidget *vbox_radio = ntk_box_new(NTK_VERTICAL, p_type);
    ntk_box_set_spacing(vbox_radio, 8);
    ntk_widget_set_margin(vbox_radio, NTK_INSETS(10, 20, 10, 20));
    ntk_box_pack_start(p_type, vbox_radio, false, false, 5);

    g_type_radio_group = ntk_radio_group_new();

    NtkWidget *rb_typical = ntk_radio_button_new("Typical Installation", vbox_radio);
    ntk_box_pack_start(vbox_radio, rb_typical, false, false, 2);
    ntk_radio_group_add(g_type_radio_group, rb_typical);

    NtkWidget *lbl_typical_info = ntk_label_new("Installs BoredOS with all standard optional packages. Recommended for most users.", vbox_radio);
    ntk_widget_set_margin(lbl_typical_info, NTK_INSETS(0, 24, 10, 0));
    ntk_box_pack_start(vbox_radio, lbl_typical_info, false, false, 2);

    NtkWidget *rb_custom = ntk_radio_button_new("Custom Installation", vbox_radio);
    ntk_box_pack_start(vbox_radio, rb_custom, false, false, 2);
    ntk_radio_group_add(g_type_radio_group, rb_custom);

    NtkWidget *lbl_custom_info = ntk_label_new("Allows you to manually select which optional packages you want to install. Recommended for advanced users.", vbox_radio);
    ntk_widget_set_margin(lbl_custom_info, NTK_INSETS(0, 24, 0, 0));
    ntk_box_pack_start(vbox_radio, lbl_custom_info, false, false, 2);

    ntk_radio_group_set_selected(g_type_radio_group, 0);
    
    NtkWidget *p_packages = ntk_box_new(NTK_VERTICAL, g_stack);
    ntk_box_set_spacing(p_packages, 8);
    ntk_stack_add_page(g_stack, p_packages, PAGE_PACKAGES);
    
    NtkWidget *lbl_pkgs_title = ntk_label_new("Select Optional Packages", p_packages);
    ntk_box_pack_start(p_packages, lbl_pkgs_title, false, false, 5);
    
    NtkWidget *lbl_pkgs_desc = ntk_label_new("Select optional components you wish to install:", p_packages);
    ntk_box_pack_start(p_packages, lbl_pkgs_desc, false, false, 2);
    
    NtkWidget *scroll_area = ntk_scroll_area_new(p_packages);
    ntk_scroll_area_set_policy(scroll_area, NTK_SCROLL_NEVER, NTK_SCROLL_AUTO);
    ntk_box_pack_start(p_packages, scroll_area, true, true, 2);
    
    NtkWidget *vbox_pkgs_list = ntk_box_new(NTK_VERTICAL, NULL);
    ntk_box_set_spacing(vbox_pkgs_list, 6);
    ntk_scroll_area_set_content(scroll_area, vbox_pkgs_list);
    
    if (g_num_options == 0) {
        ntk_label_new("No optional packages found.", vbox_pkgs_list);
    } else {
        for (int i = 0; i < g_num_options; i++) {
            g_options[i].chk_widget = ntk_checkbox_new(g_options[i].pkgname, vbox_pkgs_list);
            ntk_checkbox_set_checked(g_options[i].chk_widget, g_options[i].enabled);
            ntk_box_pack_start(vbox_pkgs_list, g_options[i].chk_widget, false, false, 2);
        }
    }
    
    NtkWidget *p_warning = ntk_box_new(NTK_VERTICAL, g_stack);
    ntk_box_set_spacing(p_warning, 8);
    ntk_stack_add_page(g_stack, p_warning, PAGE_WARNING);
    
    NtkWidget *lbl_warn_title = ntk_label_new("Developer Beta Warning", p_warning);
    ntk_box_pack_start(p_warning, lbl_warn_title, false, false, 5);
    
    NtkWidget *lbl_warn_desc = ntk_label_new("Please review the following software notice carefully:", p_warning);
    ntk_box_pack_start(p_warning, lbl_warn_desc, false, false, 2);
    
    NtkWidget *textarea = ntk_text_area_new(p_warning);
    ntk_text_area_set_text(textarea, 
        "! This is a DEVELOPER BETA build of BoredOS.\n"
        "  It will very likely always remain a beta release.\n\n"
        "! THIS SOFTWARE IS UNSTABLE.\n"
        "  It may corrupt data, destroy partitions, fail to\n"
        "  boot, or damage your system in unexpected ways.\n"
        "  The BoredOS developers are NOT responsible for\n"
        "  any damage or data loss caused by this software.\n\n"
        "! THIS OS REQUIRES COMMAND-LINE KNOWLEDGE.\n"
        "  If you have never used a shell, terminal, or CLI,\n"
        "  please do not proceed."
    );
    ntk_box_pack_start(p_warning, textarea, true, true, 2);
    
    g_chk_warning_accept = ntk_checkbox_new("I understand the warning and accept the risks.", p_warning);
    ntk_widget_connect(g_chk_warning_accept, "state-changed", on_warning_accept_changed, NULL);
    ntk_box_pack_start(p_warning, g_chk_warning_accept, false, false, 2);
    
    NtkWidget *p_confirm = ntk_box_new(NTK_VERTICAL, g_stack);
    ntk_box_set_spacing(p_confirm, 10);
    ntk_stack_add_page(g_stack, p_confirm, PAGE_CONFIRM);
    
    NtkWidget *lbl_confirm_title = ntk_label_new("Ready to Install", p_confirm);
    ntk_box_pack_start(p_confirm, lbl_confirm_title, false, false, 5);
    
    NtkWidget *lbl_confirm_desc = ntk_label_new("The assistant is ready to begin installing BoredOS.", p_confirm);
    ntk_box_pack_start(p_confirm, lbl_confirm_desc, false, false, 2);
    
    g_lbl_confirm_disk = ntk_label_new("Target Disk: /dev/...", p_confirm);
    ntk_box_pack_start(p_confirm, g_lbl_confirm_disk, false, false, 2);
    
    g_lbl_confirm_pkgs = ntk_label_new("Optional Packages: ...", p_confirm);
    ntk_box_pack_start(p_confirm, g_lbl_confirm_pkgs, false, false, 2);
    
    NtkWidget *lbl_confirm_warn = ntk_label_new("WARNING: Continuing will erase all partitions and data on the target drive!", p_confirm);
    ntk_label_set_color(lbl_confirm_warn, ntk_color_from_rgba(255, 30, 30, 255));
    ntk_box_pack_start(p_confirm, lbl_confirm_warn, false, false, 5);
    
    NtkWidget *p_progress = ntk_box_new(NTK_VERTICAL, g_stack);
    ntk_box_set_spacing(p_progress, 16);
    ntk_stack_add_page(g_stack, p_progress, PAGE_PROGRESS);
    
    NtkWidget *lbl_progress_title = ntk_label_new("Installing BoredOS", p_progress);
    ntk_box_pack_start(p_progress, lbl_progress_title, false, false, 5);
    
    g_lbl_progress_status = ntk_label_new("Initializing...", p_progress);
    ntk_box_pack_start(p_progress, g_lbl_progress_status, false, false, 2);
    
    g_progress_bar = ntk_progress_bar_new(p_progress);
    ntk_progress_bar_set_value(g_progress_bar, 0.0f);
    ntk_progress_bar_set_text(g_progress_bar, "0% Completed");
    ntk_box_pack_start(p_progress, g_progress_bar, false, false, 5);
    
    NtkWidget *p_finish = ntk_box_new(NTK_VERTICAL, g_stack);
    ntk_box_set_spacing(p_finish, 12);
    ntk_stack_add_page(g_stack, p_finish, PAGE_FINISH);
    
    NtkWidget *lbl_finish_title = ntk_label_new("Installation Complete", p_finish);
    ntk_box_pack_start(p_finish, lbl_finish_title, false, false, 5);
    
    NtkWidget *lbl_finish_desc1 = ntk_label_new("BoredOS has been successfully installed on your computer.", p_finish);
    ntk_box_pack_start(p_finish, lbl_finish_desc1, false, false, 2);
    
    NtkWidget *lbl_finish_desc2 = ntk_label_new("Please remove the installation media from your device.", p_finish);
    ntk_box_pack_start(p_finish, lbl_finish_desc2, false, false, 2);
    
    NtkWidget *lbl_finish_desc3 = ntk_label_new("Click Finish to reboot your system and start BoredOS!", p_finish);
    ntk_box_pack_start(p_finish, lbl_finish_desc3, false, false, 2);
    
    ntk_stack_set_visible_page(g_stack, PAGE_WELCOME);
    
    NtkWidget *sep_canvas = ntk_canvas_new(vbox_right);
    ntk_widget_set_min_size(sep_canvas, NTK_SIZE(480, 2));
    ntk_canvas_set_draw_callback(sep_canvas, on_separator_draw, NULL);
    ntk_box_pack_start(vbox_right, sep_canvas, false, false, 2);
    
    NtkWidget *hbox_buttons = ntk_box_new(NTK_HORIZONTAL, vbox_right);
    ntk_widget_set_margin(hbox_buttons, NTK_INSETS(10, 16, 12, 16));
    ntk_box_set_spacing(hbox_buttons, 10);
    ntk_box_pack_start(vbox_right, hbox_buttons, false, false, 0);
    
    NtkWidget *btn_spacer = ntk_widget_new(hbox_buttons);
    ntk_box_pack_start(hbox_buttons, btn_spacer, true, true, 0);
    
    g_btn_back = ntk_button_new("< Back", hbox_buttons);
    ntk_widget_connect(g_btn_back, "clicked", on_btn_back_clicked, NULL);
    ntk_box_pack_start(hbox_buttons, g_btn_back, false, false, 0);
    
    g_btn_next = ntk_button_new("Next >", hbox_buttons);
    ntk_widget_connect(g_btn_next, "clicked", on_btn_next_clicked, NULL);
    ntk_box_pack_start(hbox_buttons, g_btn_next, false, false, 0);
    
    g_btn_cancel = ntk_button_new("Cancel", hbox_buttons);
    ntk_widget_connect(g_btn_cancel, "clicked", on_btn_cancel_clicked, NULL);
    ntk_box_pack_start(hbox_buttons, g_btn_cancel, false, false, 0);
    
    update_page_state();
    
    ntk_window_center_on_screen(g_win);
    ntk_widget_show(g_win);
    
    int rc = ntk_app_run(g_app);
    
    ntk_radio_group_destroy(g_type_radio_group);
    ntk_app_destroy(g_app);
    return rc;
}
