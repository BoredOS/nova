// Copyright (c) 2026 Stryker (strykeryt001@gmail.com)
// This software is released under the GNU General Public License v3.0. See LICENSE file for details.
// This header needs to maintain in any file it is present in, as per the GPL license terms.

#include "ntk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/sound.h>

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

typedef struct {
    NtkApp *app;
    NtkWidget *win;
    NtkWidget *lbl_status;
    NtkWidget *lbl_info;
    NtkWidget *progress;
    NtkWidget *entry_path;

    uint8_t *file_data;
    size_t file_size;
    uint8_t *mp3_ptr;
    int bytes_left;

    mp3dec_t mp3d;
    int dsp_fd;
    int sample_rate;
    int channels;
    int playing;
    uint32_t timer_id;
    size_t bytes_consumed;
} Player;

static Player g_player;

static int open_dsp(int rate) {
    int fd = open("/dev/dsp", O_WRONLY);
    if (fd < 0) {
        perror("open /dev/dsp");
        return -1;
    }

    int fmt = AFMT_S16_LE;
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) < 0)
        perror("SNDCTL_DSP_SETFMT");

    int chan = 2; /* match audioplay: always stereo out */
    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &chan) < 0)
        perror("SNDCTL_DSP_CHANNELS");

    int spd = rate;
    if (ioctl(fd, SNDCTL_DSP_SPEED, &spd) < 0)
        perror("SNDCTL_DSP_SPEED");

    return fd;
}

static void player_stop(void) {
    if (g_player.timer_id) {
        ntk_app_kill_timer(g_player.timer_id);
        g_player.timer_id = 0;
    }
    if (g_player.dsp_fd >= 0) {
        ioctl(g_player.dsp_fd, SNDCTL_DSP_SYNC, NULL);
        close(g_player.dsp_fd);
        g_player.dsp_fd = -1;
    }
    g_player.playing = 0;
    if (g_player.lbl_status)
        ntk_label_set_text(g_player.lbl_status, "Stopped");
}

static void player_unload(void) {
    player_stop();
    free(g_player.file_data);
    g_player.file_data = NULL;
    g_player.file_size = 0;
    g_player.mp3_ptr = NULL;
    g_player.bytes_left = 0;
    g_player.bytes_consumed = 0;
    g_player.sample_rate = 0;
    g_player.channels = 0;
}

static int player_load(const char *path) {
    player_unload();

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        perror("open mp3");
        return -1;
    }

    off_t size = lseek(fd, 0, SEEK_END);
    if (size <= 0) {
        close(fd);
        return -1;
    }
    lseek(fd, 0, SEEK_SET);

    uint8_t *buf = malloc((size_t)size);
    if (!buf) {
        close(fd);
        return -1;
    }
    if (read(fd, buf, (size_t)size) != size) {
        free(buf);
        close(fd);
        return -1;
    }
    close(fd);

    g_player.file_data = buf;
    g_player.file_size = (size_t)size;
    g_player.mp3_ptr = buf;
    g_player.bytes_left = (int)size;
    g_player.bytes_consumed = 0;
    mp3dec_init(&g_player.mp3d);

    char info[256];
    snprintf(info, sizeof(info), "Loaded: %s (%zu bytes)", path, (size_t)size);
    ntk_label_set_text(g_player.lbl_info, info);
    ntk_label_set_text(g_player.lbl_status, "Ready");
    ntk_progress_bar_set_value(g_player.progress, 0.0f);
    return 0;
}

/* Decode a few frames per timer tick so the UI stays responsive */
static void on_play_tick(uint32_t timer_id, void *userdata) {
    (void)timer_id;
    (void)userdata;
    if (!g_player.playing || !g_player.file_data)
        return;

    mp3dec_frame_info_t info;
    int16_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int frames_this_tick = 0;
    const int max_frames = 8;

    while (g_player.bytes_left > 0 && frames_this_tick < max_frames) {
        int samples = mp3dec_decode_frame(
            &g_player.mp3d,
            g_player.mp3_ptr,
            g_player.bytes_left,
            pcm,
            &info
        );

        if (info.frame_bytes > 0) {
            g_player.mp3_ptr += info.frame_bytes;
            g_player.bytes_left -= info.frame_bytes;
            g_player.bytes_consumed += (size_t)info.frame_bytes;
        } else {
            g_player.mp3_ptr++;
            g_player.bytes_left--;
            g_player.bytes_consumed++;
            continue;
        }

        if (samples <= 0)
            continue;

        if (g_player.dsp_fd < 0) {
            g_player.sample_rate = info.hz;
            g_player.channels = info.channels;
            g_player.dsp_fd = open_dsp(info.hz);
            if (g_player.dsp_fd < 0) {
                player_stop();
                ntk_label_set_text(g_player.lbl_status, "Failed to open /dev/dsp");
                return;
            }
            char info_txt[128];
            snprintf(info_txt, sizeof(info_txt),
                     "Playing — %d Hz, %d ch", info.hz, info.channels);
            ntk_label_set_text(g_player.lbl_info, info_txt);
        }

        if (info.channels == 1) {
            int out_bytes = samples * 2 * (int)sizeof(int16_t);
            int16_t *dst = malloc((size_t)out_bytes);
            if (dst) {
                for (int i = 0; i < samples; i++) {
                    dst[i * 2] = pcm[i];
                    dst[i * 2 + 1] = pcm[i];
                }
                write(g_player.dsp_fd, dst, (size_t)out_bytes);
                free(dst);
            }
        } else {
            write(g_player.dsp_fd, pcm,
                  (size_t)(samples * info.channels * (int)sizeof(int16_t)));
        }

        frames_this_tick++;
    }

    if (g_player.file_size > 0) {
        float p = (float)g_player.bytes_consumed / (float)g_player.file_size;
        if (p > 1.0f) p = 1.0f;
        ntk_progress_bar_set_value(g_player.progress, p);
    }

    if (g_player.bytes_left <= 0) {
        player_stop();
        ntk_label_set_text(g_player.lbl_status, "Finished");
        ntk_progress_bar_set_value(g_player.progress, 1.0f);
    }
}

static void player_start(void) {
    if (!g_player.file_data) {
        ntk_dialog_message(g_player.win, "MP3 Player",
                           "Load an MP3 file first.", NTK_MSG_INFO,
                           NTK_DIALOG_BUTTONS_OK);
        return;
    }
    if (g_player.playing)
        return;

    g_player.mp3_ptr = g_player.file_data;
    g_player.bytes_left = (int)g_player.file_size;
    g_player.bytes_consumed = 0;
    mp3dec_init(&g_player.mp3d);
    if (g_player.dsp_fd >= 0) {
        close(g_player.dsp_fd);
        g_player.dsp_fd = -1;
    }

    g_player.playing = 1;
    ntk_label_set_text(g_player.lbl_status, "Playing…");
    g_player.timer_id = ntk_app_set_timer(20, on_play_tick, NULL);
}

static void on_load_clicked(NtkWidget *w, void *userdata) {
    (void)w;
    (void)userdata;

    const char *path = ntk_text_entry_get_text(g_player.entry_path);
    if (!path || !path[0]) {
        char *picked = ntk_dialog_get_text(
            g_player.win, "Open MP3",
            "Enter path to .mp3 file:", "/");
        if (!picked)
            return;
        ntk_text_entry_set_text(g_player.entry_path, picked);
        if (player_load(picked) != 0) {
            ntk_dialog_message(g_player.win, "Error",
                               "Could not load file.", NTK_MSG_INFO,
                               NTK_DIALOG_BUTTONS_OK);
        }
        free(picked);
        return;
    }
    if (player_load(path) != 0) {
        ntk_dialog_message(g_player.win, "Error",
                           "Could not load file.", NTK_MSG_INFO,
                           NTK_DIALOG_BUTTONS_OK);
    }
}

static void on_play_clicked(NtkWidget *w, void *userdata) {
    (void)w;
    (void)userdata;
    player_start();
}

static void on_stop_clicked(NtkWidget *w, void *userdata) {
    (void)w;
    (void)userdata;
    player_stop();
}

int main(int argc, char **argv) {
    memset(&g_player, 0, sizeof(g_player));
    g_player.dsp_fd = -1;

    g_player.app = ntk_app_new();
    if (!g_player.app)
        return 1;

    g_player.win = ntk_window_new("MP3 Player", 420, 220);
    if (!g_player.win) {
        ntk_app_destroy(g_player.app);
        return 1;
    }

    NtkWidget *vbox = ntk_box_new(NTK_VERTICAL, g_player.win);
    ntk_window_set_content(g_player.win, vbox);
    ntk_box_set_spacing(vbox, 6);

    g_player.lbl_status = ntk_label_new("Stopped", vbox);
    ntk_box_pack_start(vbox, g_player.lbl_status, false, false, 4);

    g_player.lbl_info = ntk_label_new("No file loaded", vbox);
    ntk_box_pack_start(vbox, g_player.lbl_info, false, false, 4);

    NtkWidget *path_row = ntk_box_new(NTK_HORIZONTAL, vbox);
    ntk_box_pack_start(vbox, path_row, false, false, 4);

    g_player.entry_path = ntk_text_entry_new(path_row);
    ntk_box_pack_start(path_row, g_player.entry_path, true, true, 4);

    NtkWidget *btn_load = ntk_button_new("Load", path_row);
    ntk_widget_connect(btn_load, "clicked", on_load_clicked, NULL);
    ntk_box_pack_start(path_row, btn_load, false, false, 4);

    g_player.progress = ntk_progress_bar_new(vbox);
    ntk_progress_bar_set_value(g_player.progress, 0.0f);
    ntk_box_pack_start(vbox, g_player.progress, false, false, 4);

    NtkWidget *ctrl = ntk_box_new(NTK_HORIZONTAL, vbox);
    ntk_box_pack_start(vbox, ctrl, false, false, 4);

    NtkWidget *btn_play = ntk_button_new("Play", ctrl);
    ntk_widget_connect(btn_play, "clicked", on_play_clicked, NULL);
    ntk_box_pack_start(ctrl, btn_play, true, true, 4);

    NtkWidget *btn_stop = ntk_button_new("Stop", ctrl);
    ntk_widget_connect(btn_stop, "clicked", on_stop_clicked, NULL);
    ntk_box_pack_start(ctrl, btn_stop, true, true, 4);

    if (argc >= 2) {
        ntk_text_entry_set_text(g_player.entry_path, argv[1]);
        player_load(argv[1]);
    }

    ntk_widget_show(g_player.win);
    int rc = ntk_app_run(g_player.app);

    player_unload();
    ntk_app_destroy(g_player.app);
    return rc;
}
