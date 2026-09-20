#ifndef PSVITAAUTO_MOCK_PHONE_H
#define PSVITAAUTO_MOCK_PHONE_H

typedef struct mock_phone mock_phone;

typedef struct {
    int video_open_ok;
    int input_open_ok;
    int audio_open_ok;
    int setup_ok;
    int focus_ok;
    int video_ack_ok;
    int audio_ack_ok;
    int binding_ok;
    int ping_ok;
    int video_resolution;
    int video_fps;
} mock_phone_result;

mock_phone *mock_phone_create(int fd);
void mock_phone_destroy(mock_phone *p);

/* Drive one step: process any inbound message, then send any proactive
 * message. Call repeatedly until mock_phone_done() is true. */
void mock_phone_step(mock_phone *p);
int mock_phone_done(const mock_phone *p);
void mock_phone_result_get(const mock_phone *p, mock_phone_result *r);

#endif
