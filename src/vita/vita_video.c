#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psp2/kernel/processmgr.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/ctrl.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>
#include <psp2/touch.h>
#include <psp2/io/fcntl.h>

#include <vita2d.h>
#include <psp2/gxm.h>
#include <debugnet.h>

#include "aa_constants.h"
#include "control.h"
#include "discover.h"
#include "transport.h"
#include "tls.h"
#include "avcdec.h"

#define DBGNET_HOST "192.168.1.100"
#define DBGNET_PORT 18194

/* Phone head unit server (default; override via ux0:data/psvitaauto.cfg "ip:port"). */
#define PHONE_HOST "192.168.1.16"
#define PHONE_PORT 5277

static void load_config(char *host, size_t host_cap, int *port) {
    strcpy(host, PHONE_HOST);
    *port = PHONE_PORT;

    int fd = sceIoOpen("ux0:data/psvitaauto.cfg", SCE_O_RDONLY, 0);
    if (fd < 0) {
        return;
    }
    char buf[128];
    int n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n <= 0) {
        return;
    }
    buf[n] = 0;
    char *colon = strchr(buf, ':');
    if (colon) {
        *colon = 0;
        strncpy(host, buf, host_cap - 1);
        host[host_cap - 1] = 0;
        *port = atoi(colon + 1);
    } else {
        strncpy(host, buf, host_cap - 1);
        host[host_cap - 1] = 0;
    }
}

#define VIDEO_WIDTH 1280
#define VIDEO_HEIGHT 720

#define SCREEN_WIDTH 960
#define SCREEN_HEIGHT 544

static avcdec *decoder = NULL;
static vita2d_texture *frame_tex[2] = { NULL, NULL };
static int ready_tex = 0;
static int video_frames = 0;
static int video_cbs = 0;
static int decode_err = 0;
static uint64_t video_bytes = 0;

/* Encoded access-unit queue feeding the decode thread. */
#define AU_SLOT_COUNT 4
#define AU_SLOT_SIZE (128 * 1024)
static struct {
    uint8_t data[AU_SLOT_SIZE];
    size_t len;
} au_slots[AU_SLOT_COUNT];
static pthread_mutex_t au_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t au_cond = PTHREAD_COND_INITIALIZER;
static int au_read = 0;
static int au_write = 0;
static int au_count = 0;

/* AA keycodes (aap_protobuf KeyCode enum). */
#define AA_KEY_DPAD_UP      19
#define AA_KEY_DPAD_DOWN    20
#define AA_KEY_DPAD_LEFT    21
#define AA_KEY_DPAD_RIGHT   22
#define AA_KEY_DPAD_CENTER  23
#define AA_KEY_HOME         3
#define AA_KEY_BACK         4
#define AA_KEY_BUTTON_L1    102
#define AA_KEY_BUTTON_R1    103
#define AA_KEY_BUTTON_START 108

static uint32_t prev_buttons = 0;
static int prev_touch = 0;
static int prev_touch_x = 0;
static int prev_touch_y = 0;

static void send_key(aa_control *c, uint32_t keycode, int down) {
    aa_control_send_key(c, sceKernelGetProcessTimeWide(), keycode, down);
}

static void handle_input(aa_control *c) {
    SceCtrlData pad;
    sceCtrlPeekBufferPositive(0, &pad, 1);

    uint32_t changed = prev_buttons ^ pad.buttons;
    static const uint32_t keys[][2] = {
        { SCE_CTRL_CROSS,   AA_KEY_DPAD_CENTER },
        { SCE_CTRL_CIRCLE,  AA_KEY_BACK },
        { SCE_CTRL_TRIANGLE, AA_KEY_HOME },
        { SCE_CTRL_UP,      AA_KEY_DPAD_UP },
        { SCE_CTRL_DOWN,    AA_KEY_DPAD_DOWN },
        { SCE_CTRL_LEFT,    AA_KEY_DPAD_LEFT },
        { SCE_CTRL_RIGHT,   AA_KEY_DPAD_RIGHT },
        { SCE_CTRL_LTRIGGER, AA_KEY_BUTTON_L1 },
        { SCE_CTRL_RTRIGGER, AA_KEY_BUTTON_R1 },
        { SCE_CTRL_START,   AA_KEY_BUTTON_START },
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (changed & keys[i][0]) {
            send_key(c, keys[i][1], (pad.buttons & keys[i][0]) != 0);
        }
    }
    prev_buttons = pad.buttons;

    SceTouchData touch;
    memset(&touch, 0, sizeof(touch));
    sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

    if (touch.reportNum > 0) {
        int x = touch.report[0].x * 2 / 3;       /* 1920 -> 1280 */
        int y = touch.report[0].y * 720 / 1088;  /* 1088 -> 720 */
        uint8_t action;
        if (!prev_touch) {
            action = 0; /* ACTION_DOWN */
        } else {
            action = 2; /* ACTION_MOVED */
        }
        aa_control_send_touch(c, sceKernelGetProcessTimeWide(), (uint32_t)x, (uint32_t)y,
                              action, 0);
        prev_touch = 1;
        prev_touch_x = x;
        prev_touch_y = y;
    } else if (prev_touch) {
        aa_control_send_touch(c, sceKernelGetProcessTimeWide(),
                              (uint32_t)prev_touch_x, (uint32_t)prev_touch_y,
                              1 /* ACTION_UP */, 0);
        prev_touch = 0;
    }
}

/* --- Annex-B access-unit splitter (simplified) --- */
#define AU_BUF_MAX (512 * 1024)
static uint8_t au_buf[AU_BUF_MAX];
static size_t au_len = 0;

static int is_start_code(const uint8_t *p, size_t len) {
    return (len >= 3 && p[0] == 0 && p[1] == 0 && p[2] == 1) ||
           (len >= 4 && p[0] == 0 && p[1] == 0 && p[2] == 0 && p[3] == 1);
}

static void feed_decoder(uint8_t *au, size_t n) {
    if (n == 0 || n > AU_SLOT_SIZE) {
        return;
    }
    pthread_mutex_lock(&au_mutex);
    if (au_count >= AU_SLOT_COUNT) {
        /* Decoder is falling behind: drop the newest frame rather than grow
         * the queue (keeps the decode stream uncorrupted). */
        pthread_mutex_unlock(&au_mutex);
        return;
    }
    au_slots[au_write].len = n;
    memcpy(au_slots[au_write].data, au, n);
    au_write = (au_write + 1) % AU_SLOT_COUNT;
    au_count++;
    pthread_cond_signal(&au_cond);
    pthread_mutex_unlock(&au_mutex);
}

static void *decode_thread(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&au_mutex);
        while (au_count == 0) {
            pthread_cond_wait(&au_cond, &au_mutex);
        }
        int idx = au_read;
        uint8_t *data = au_slots[idx].data;
        size_t len = au_slots[idx].len;
        pthread_mutex_unlock(&au_mutex);

        int back = __atomic_load_n(&ready_tex, __ATOMIC_ACQUIRE) ^ 1;
        avcdec_set_output(decoder, vita2d_texture_get_datap(frame_tex[back]),
                          SCREEN_WIDTH, SCREEN_HEIGHT);
        if (avcdec_decode(decoder, data, (uint32_t)len) == 0) {
            __atomic_store_n(&ready_tex, back, __ATOMIC_RELEASE);
            video_frames++;
        } else {
            decode_err++;
        }

        pthread_mutex_lock(&au_mutex);
        au_read = (au_read + 1) % AU_SLOT_COUNT;
        au_count--;
        pthread_mutex_unlock(&au_mutex);
    }
    return NULL;
}

/* Split the Annex-B stream into access units and feed them to the decoder.
 * An access unit ends after a VCL NAL (slice); non-VCL NALs (SPS/PPS/SEI)
 * preceding it belong to that AU. */
static void handle_video(const uint8_t *data, size_t len) {
    if (au_len + len > AU_BUF_MAX) {
        au_len = 0;
        return;
    }
    memcpy(au_buf + au_len, data, len);
    au_len += len;

    size_t au_start = 0;
    size_t i = 0;
    int cur_has_vcl = 0;

    while (i < au_len) {
        if (is_start_code(au_buf + i, au_len - i)) {
            size_t sc_len = (au_buf[i + 2] == 1) ? 3 : 4;
            size_t nal_hdr = i + sc_len;
            if (nal_hdr < au_len) {
                uint8_t nal_type = au_buf[nal_hdr] & 0x1F;
                int is_vcl = (nal_type >= 1 && nal_type <= 5);
                if (is_vcl && cur_has_vcl) {
                    /* previous AU (au_start..i) is complete: feed it */
                    feed_decoder(au_buf + au_start, i - au_start);
                    au_start = i;
                    cur_has_vcl = 0;
                }
                if (is_vcl) {
                    cur_has_vcl = 1;
                }
            }
            i = nal_hdr;
        } else {
            i++;
        }
    }

    /* keep the trailing (incomplete) AU in the buffer */
    if (au_start > 0) {
        memmove(au_buf, au_buf + au_start, au_len - au_start);
        au_len -= au_start;
    }
}

static void video_cb(void *ctx, uint64_t ts, const uint8_t *h264, size_t len) {
    (void)ctx;
    (void)ts;
    video_cbs++;
    video_bytes += len;
    handle_video(h264, len);
}

static void draw_status(vita2d_pgf *pgf, const char *line) {
    vita2d_start_drawing();
    vita2d_clear_screen();
    if (pgf) {
        vita2d_pgf_draw_text(pgf, 20, 20, RGBA8(255, 255, 255, 255), 1.0f, line);
    }
    vita2d_end_drawing();
    vita2d_swap_buffers();
}

static void show_error(vita2d_pgf *pgf, const char *msg) {
    char buf[256];
    snprintf(buf, sizeof(buf), "ERROR: %s", msg);
    draw_status(pgf, buf);
    sceKernelDelayThread(3 * 1000 * 1000);
}

int main(void) {
    debugNetInit(DBGNET_HOST, DBGNET_PORT, DEBUG);
    debugNetPrintf(DEBUG, "PSVitaAuto: vita-video starting\n");

    sceNetInit(0);
    sceNetCtlInit();
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    vita2d_init();
    vita2d_set_clear_color(RGBA8(0, 0, 0, 255));
    vita2d_pgf *pgf = vita2d_load_default_pgf();

    char host[64];
    char status[256];
    int port = PHONE_PORT;
    int derr = 0;

    draw_status(pgf, "discovering phone...");
    if (aa_discover_head_unit((uint16_t)port, host, sizeof(host)) == 0) {
        debugNetPrintf(DEBUG, "PSVitaAuto: discovered phone at %s\n", host);
        /* The probe's connect/close touched the single-connection server;
         * give it a beat to reset before the real connection. */
        sceKernelDelayThread(500 * 1000);
    } else {
        load_config(host, sizeof(host), &port);
        debugNetPrintf(DEBUG, "PSVitaAuto: no phone discovered, using %s:%d\n", host, port);
    }

    snprintf(status, sizeof(status), "connecting to %s:%d...", host, port);
    draw_status(pgf, status);

    aa_transport t;
    if (aa_transport_connect(&t, host, (uint16_t)port) != 0) {
        debugNetPrintf(ERROR, "PSVitaAuto: connect to %s:%d failed\n", host, port);
        show_error(pgf, "connect failed");
        goto done;
    }
    debugNetPrintf(DEBUG, "PSVitaAuto: connected to %s:%d\n", host, port);

    draw_status(pgf, "connected, starting decoder...");

    decoder = avcdec_create(VIDEO_WIDTH, VIDEO_HEIGHT, 2, &derr);
    if (!decoder) {
        debugNetPrintf(ERROR, "PSVitaAuto: avcdec_create failed (step %d, 0x%08X)\n",
                       avcdec_last_step(), avcdec_last_rc());
        char msg[128];
        snprintf(msg, sizeof(msg), "decoder step %d failed 0x%08X (mem=%u)",
                 avcdec_last_step(), avcdec_last_rc(), avcdec_last_framesize());
        show_error(pgf, msg);
        goto done;
    }

    draw_status(pgf, "starting control channel...");

    frame_tex[0] = vita2d_create_empty_texture_format(SCREEN_WIDTH, SCREEN_HEIGHT,
                                                      SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
    frame_tex[1] = vita2d_create_empty_texture_format(SCREEN_WIDTH, SCREEN_HEIGHT,
                                                      SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR);
    if (!frame_tex[0] || !frame_tex[1]) {
        show_error(pgf, "texture create failed");
        goto done;
    }
    avcdec_set_output(decoder, vita2d_texture_get_datap(frame_tex[0]),
                      SCREEN_WIDTH, SCREEN_HEIGHT);

    pthread_t decode_tid;
    pthread_create(&decode_tid, NULL, decode_thread, NULL);

    aa_control *control = aa_control_create(&t);
    if (!control) {
        debugNetPrintf(ERROR, "PSVitaAuto: aa_control_create failed (tls step %d, 0x%04X)\n",
                       aa_tls_last_step(), aa_tls_last_rc());
        char msg[128];
        snprintf(msg, sizeof(msg), "control init failed (tls step %d, 0x%04X)",
                 aa_tls_last_step(), aa_tls_last_rc());
        show_error(pgf, msg);
        goto done;
    }
    aa_control_set_video_callback(control, video_cb, NULL);
    aa_control_set_log(control, 1);

    draw_status(pgf, "handshaking with phone...");

    SceCtrlData pad;
    memset(&pad, 0, sizeof(pad));

    while (1) {
        sceCtrlPeekBufferPositive(0, &pad, 1);
        if (pad.buttons & SCE_CTRL_START) {
            break;
        }

        aa_control_poll(control);
        handle_input(control);

        vita2d_start_drawing();
        vita2d_clear_screen();
        int rt = __atomic_load_n(&ready_tex, __ATOMIC_ACQUIRE);
        if (frame_tex[rt]) {
            vita2d_draw_texture(frame_tex[rt], 0, 0);
        }
        if (pgf) {
            snprintf(status, sizeof(status), "%s  st %d", host,
                     aa_control_get_stage(control));
            vita2d_pgf_draw_text(pgf, 20, 20, RGBA8(255, 255, 255, 255), 1.0f, status);
            snprintf(status, sizeof(status), "cbs %d, %lluB, fr %d, err %d, rx %llu",
                     video_cbs, (unsigned long long)video_bytes, video_frames, decode_err,
                     (unsigned long long)t.rx_bytes);
            vita2d_pgf_draw_text(pgf, 20, 50, RGBA8(200, 200, 200, 255), 1.0f, status);
        }
        vita2d_end_drawing();
        vita2d_swap_buffers();
    }

    aa_control_destroy(control);

done:
    if (decoder) {
        avcdec_destroy(decoder);
    }
    if (frame_tex[0]) {
        vita2d_free_texture(frame_tex[0]);
    }
    if (frame_tex[1]) {
        vita2d_free_texture(frame_tex[1]);
    }
    aa_transport_close(&t);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_STOP);
    if (pgf) {
        vita2d_free_pgf(pgf);
    }
    vita2d_fini();
    sceNetCtlTerm();
    sceNetTerm();

    debugNetPrintf(DEBUG, "PSVitaAuto: exiting\n");
    debugNetFinish();
    sceKernelExitProcess(0);
    return 0;
}
