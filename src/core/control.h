#ifndef PSVITAAUTO_CONTROL_H
#define PSVITAAUTO_CONTROL_H

#include "transport.h"

typedef struct aa_control aa_control;

typedef enum {
    AA_CONTROL_IDLE = 0,
    AA_CONTROL_ACTIVE,
    AA_CONTROL_FAILED
} aa_control_state;

/* Create a head-unit control channel over an already-connected transport. */
aa_control *aa_control_create(aa_transport *t);
void aa_control_destroy(aa_control *c);

/* Drive the handshake: reads inbound bytes, advances the state machine,
 * sends responses. Returns the current state. */
aa_control_state aa_control_poll(aa_control *c);
aa_control_state aa_control_get_state(const aa_control *c);

/* Handshake stage for diagnostics:
 * 0=SEND_VERSION 1=WAIT_VERSION 2=TLS 3=WAIT_SERVICE_DISCOVERY 4=ACTIVE 5=FAILED */
int aa_control_get_stage(const aa_control *c);

/* Register a callback for decoded H.264 video frames (timestamp + Annex-B). */
typedef void (*aa_video_frame_cb)(void *ctx, uint64_t timestamp,
                                  const uint8_t *h264, size_t len);
void aa_control_set_video_callback(aa_control *c, aa_video_frame_cb cb, void *ctx);

/* Advertised video config (default 720p30). Values match the modern AA enums. */
typedef enum { AA_VIDEO_480P = 1, AA_VIDEO_720P = 2, AA_VIDEO_1080P = 3 } aa_video_resolution;
typedef enum { AA_FPS_60 = 1, AA_FPS_30 = 2 } aa_video_fps;
void aa_control_set_video_config(aa_control *c, aa_video_resolution res, aa_video_fps fps);

/* Enable stderr diagnostics of handshake progress (default off). */
void aa_control_set_log(aa_control *c, int enabled);

/* Input forwarding (coordinates in the advertised video-frame space). */
int aa_control_send_touch(aa_control *c, uint64_t ts_us, uint32_t x, uint32_t y,
                          uint8_t action, uint32_t pointer_id);
int aa_control_send_key(aa_control *c, uint64_t ts_us, uint32_t keycode, int down);

#endif
