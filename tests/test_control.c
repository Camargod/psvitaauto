#include "test.h"

#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "control.h"
#include "mock_phone.h"
#include "transport.h"

static uint8_t received_h264[256];
static size_t received_h264_len = 0;
static uint64_t received_ts = 0;

static void video_cb(void *ctx, uint64_t ts, const uint8_t *h264, size_t len) {
    (void)ctx;
    received_ts = ts;
    received_h264_len = len < sizeof(received_h264) ? len : sizeof(received_h264);
    memcpy(received_h264, h264, received_h264_len);
}

static void drive_to_done(aa_control *hu, mock_phone *ph) {
    int iter = 0;
    while (iter++ < 2000) {
        mock_phone_step(ph);
        aa_control_state s = aa_control_poll(hu);
        if (mock_phone_done(ph)) {
            break;
        }
        if (s == AA_CONTROL_FAILED) {
            break;
        }
    }
}

static void test_full_handshake(void) {
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    aa_transport hu_t;
    hu_t.fd = fds[0];

    mock_phone *ph = mock_phone_create(fds[1]);
    TEST_ASSERT_TRUE(ph != NULL);

    aa_control *hu = aa_control_create(&hu_t);
    TEST_ASSERT_TRUE(hu != NULL);
    aa_control_set_video_callback(hu, video_cb, NULL);

    drive_to_done(hu, ph);

    mock_phone_result r;
    mock_phone_result_get(ph, &r);

    TEST_ASSERT_EQUAL_INT(AA_CONTROL_ACTIVE, aa_control_get_state(hu));
    TEST_ASSERT_TRUE(r.video_open_ok);
    TEST_ASSERT_TRUE(r.input_open_ok);
    TEST_ASSERT_TRUE(r.audio_open_ok);
    TEST_ASSERT_TRUE(r.setup_ok);
    TEST_ASSERT_TRUE(r.focus_ok);
    TEST_ASSERT_TRUE(r.video_ack_ok);
    TEST_ASSERT_TRUE(r.audio_ack_ok);
    TEST_ASSERT_TRUE(r.binding_ok);
    TEST_ASSERT_TRUE(r.ping_ok);

    TEST_ASSERT_EQUAL_INT(0, (int)received_ts);
    TEST_ASSERT_EQUAL_INT(7, (int)received_h264_len);
    TEST_ASSERT_TRUE(received_h264[0] == 0x00 && received_h264[4] == 0x65);

    aa_control_destroy(hu);
    mock_phone_destroy(ph);
    close(fds[0]);
    close(fds[1]);
}

static void test_video_config(void) {
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    aa_transport hu_t;
    hu_t.fd = fds[0];

    mock_phone *ph = mock_phone_create(fds[1]);
    TEST_ASSERT_TRUE(ph != NULL);

    aa_control *hu = aa_control_create(&hu_t);
    TEST_ASSERT_TRUE(hu != NULL);
    aa_control_set_video_config(hu, AA_VIDEO_480P, AA_FPS_60);

    drive_to_done(hu, ph);

    mock_phone_result r;
    mock_phone_result_get(ph, &r);
    TEST_ASSERT_EQUAL_INT(AA_VIDEO_480P, r.video_resolution);
    TEST_ASSERT_EQUAL_INT(AA_FPS_60, r.video_fps);

    aa_control_destroy(hu);
    mock_phone_destroy(ph);
    close(fds[0]);
    close(fds[1]);
}

int main(void) {
    TEST_RUN(test_full_handshake);
    TEST_RUN(test_video_config);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
