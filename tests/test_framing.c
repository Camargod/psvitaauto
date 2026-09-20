#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "aa_constants.h"
#include "frame.h"
#include "stream.h"

static void test_bulk_roundtrip(void) {
    uint8_t payload[16];
    for (int i = 0; i < 16; i++) {
        payload[i] = (uint8_t)i;
    }

    aa_frame f;
    f.channel_id = AA_CHANNEL_CONTROL;
    f.frame_type = AA_FRAME_BULK;
    f.message_type = AA_MSG_CONTROL;
    f.encryption_type = AA_ENC_PLAIN;
    f.payload = payload;
    f.payload_size = 16;
    f.total_message_size = 0;

    uint8_t buf[64];
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_frame_encode(&f, buf, sizeof(buf), &len));

    aa_frame out;
    size_t consumed = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_frame_decode(buf, len, &out, &consumed));
    TEST_ASSERT_EQUAL_INT(AA_CHANNEL_CONTROL, out.channel_id);
    TEST_ASSERT_EQUAL_INT(AA_FRAME_BULK, out.frame_type);
    TEST_ASSERT_EQUAL_INT(AA_MSG_CONTROL, out.message_type);
    TEST_ASSERT_EQUAL_INT(AA_ENC_PLAIN, out.encryption_type);
    TEST_ASSERT_EQUAL_INT(16, out.payload_size);
    TEST_ASSERT_TRUE(memcmp(out.payload, payload, 16) == 0);
    TEST_ASSERT_EQUAL_INT((int)len, (int)consumed);
}

static void test_length_encoding(void) {
    uint8_t payload[4] = {1, 2, 3, 4};

    aa_frame f;
    f.channel_id = AA_CHANNEL_VIDEO;
    f.message_type = AA_MSG_SPECIFIC;
    f.encryption_type = AA_ENC_PLAIN;
    f.payload = payload;
    f.payload_size = 4;
    f.total_message_size = 4;

    uint8_t buf[64];
    size_t len = 0;

    f.frame_type = AA_FRAME_FIRST;
    TEST_ASSERT_EQUAL_INT(0, aa_frame_encode(&f, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_INT(2 + 6 + 4, (int)len);

    f.frame_type = AA_FRAME_LAST;
    TEST_ASSERT_EQUAL_INT(0, aa_frame_encode(&f, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_INT(2 + 2 + 4, (int)len);

    f.frame_type = AA_FRAME_BULK;
    TEST_ASSERT_EQUAL_INT(0, aa_frame_encode(&f, buf, sizeof(buf), &len));
    TEST_ASSERT_EQUAL_INT(2 + 2 + 4, (int)len);
}

typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t len;
} buffer_sink;

static int sink_write(void *ctx, const uint8_t *data, size_t len) {
    buffer_sink *s = ctx;
    if (s->len + len > s->cap) {
        return -1;
    }
    memcpy(s->buf + s->len, data, len);
    s->len += len;
    return 0;
}

static void test_fragmentation_roundtrip(void) {
    size_t msg_len = AA_MAX_FRAME_PAYLOAD * 2 + 100;
    uint8_t *msg = malloc(msg_len);
    TEST_ASSERT_TRUE(msg != NULL);
    for (size_t i = 0; i < msg_len; i++) {
        msg[i] = (uint8_t)(i * 7 + 3);
    }

    uint8_t *wire = malloc(msg_len + 1024);
    TEST_ASSERT_TRUE(wire != NULL);

    buffer_sink s = {wire, msg_len + 1024, 0};
    TEST_ASSERT_EQUAL_INT(0, aa_stream_write_message(AA_CHANNEL_INPUT, AA_MSG_SPECIFIC,
                                                     AA_ENC_PLAIN, msg, msg_len,
                                                     sink_write, &s));

    aa_stream_reader r;
    aa_stream_reader_init(&r);
    TEST_ASSERT_EQUAL_INT(0, aa_stream_reader_feed(&r, s.buf, s.len));

    uint8_t *out_msg = NULL;
    size_t out_len = 0;
    uint8_t out_channel = 0;
    uint8_t out_mtype = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_stream_reader_next_message(&r, &out_msg, &out_len,
                                                           &out_channel, &out_mtype));
    TEST_ASSERT_EQUAL_INT((int)msg_len, (int)out_len);
    TEST_ASSERT_EQUAL_INT(AA_CHANNEL_INPUT, out_channel);
    TEST_ASSERT_EQUAL_INT(AA_MSG_SPECIFIC, out_mtype);
    TEST_ASSERT_TRUE(memcmp(out_msg, msg, msg_len) == 0);

    aa_stream_reader_free(&r);
    free(msg);
    free(wire);
}

static void test_feed_in_chunks(void) {
    uint8_t msg[100];
    for (int i = 0; i < 100; i++) {
        msg[i] = (uint8_t)(255 - i);
    }

    uint8_t wire[256];
    buffer_sink s = {wire, sizeof(wire), 0};
    TEST_ASSERT_EQUAL_INT(0, aa_stream_write_message(AA_CHANNEL_VIDEO, AA_MSG_SPECIFIC,
                                                     AA_ENC_PLAIN, msg, sizeof(msg),
                                                     sink_write, &s));

    aa_stream_reader r;
    aa_stream_reader_init(&r);
    for (size_t i = 0; i < s.len; i++) {
        TEST_ASSERT_EQUAL_INT(0, aa_stream_reader_feed(&r, s.buf + i, 1));
    }

    uint8_t *out_msg = NULL;
    size_t out_len = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_stream_reader_next_message(&r, &out_msg, &out_len, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(100, (int)out_len);
    TEST_ASSERT_TRUE(memcmp(out_msg, msg, sizeof(msg)) == 0);

    aa_stream_reader_free(&r);
}

int main(void) {
    TEST_RUN(test_bulk_roundtrip);
    TEST_RUN(test_length_encoding);
    TEST_RUN(test_fragmentation_roundtrip);
    TEST_RUN(test_feed_in_chunks);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
