#include "stream.h"

#include <stdlib.h>
#include <string.h>

#include "frame.h"

void aa_stream_reader_init(aa_stream_reader *r) {
    memset(r, 0, sizeof(*r));
}

void aa_stream_reader_free(aa_stream_reader *r) {
    free(r->in);
    free(r->msg);
    memset(r, 0, sizeof(*r));
}

static int buf_reserve(uint8_t **buf, size_t *cap, size_t need) {
    if (need <= *cap) {
        return 0;
    }
    size_t ncap = *cap ? *cap : 256;
    while (ncap < need) {
        ncap *= 2;
    }
    uint8_t *nb = realloc(*buf, ncap);
    if (!nb) {
        return -1;
    }
    *buf = nb;
    *cap = ncap;
    return 0;
}

int aa_stream_reader_feed(aa_stream_reader *r, const uint8_t *data, size_t len) {
    if (len == 0) {
        return 0;
    }
    if (buf_reserve(&r->in, &r->in_cap, r->in_len + len) != 0) {
        return -1;
    }
    memcpy(r->in + r->in_len, data, len);
    r->in_len += len;
    return 0;
}

int aa_stream_reader_next_message(aa_stream_reader *r,
                                  uint8_t **out_msg, size_t *out_msg_len,
                                  uint8_t *out_channel, uint8_t *out_message_type) {
    while (r->in_len > 0) {
        aa_frame f;
        size_t consumed = 0;
        if (aa_frame_decode(r->in, r->in_len, &f, &consumed) != 0) {
            return 1;
        }

        if (f.frame_type == AA_FRAME_FIRST) {
            r->msg_total = f.total_message_size;
            r->msg_len = 0;
            r->msg_channel = f.channel_id;
            r->msg_message_type = f.message_type;
            r->in_message = 1;
        } else if (!r->in_message) {
            r->msg_total = (uint32_t)f.payload_size;
            r->msg_len = 0;
            r->msg_channel = f.channel_id;
            r->msg_message_type = f.message_type;
            r->in_message = 1;
        }

        if (buf_reserve(&r->msg, &r->msg_cap, r->msg_len + f.payload_size) != 0) {
            return -1;
        }
        memcpy(r->msg + r->msg_len, f.payload, f.payload_size);
        r->msg_len += f.payload_size;

        memmove(r->in, r->in + consumed, r->in_len - consumed);
        r->in_len -= consumed;

        if (r->in_message && r->msg_len >= r->msg_total) {
            r->in_message = 0;
            *out_msg = r->msg;
            *out_msg_len = r->msg_len;
            if (out_channel) {
                *out_channel = r->msg_channel;
            }
            if (out_message_type) {
                *out_message_type = r->msg_message_type;
            }
            return 0;
        }
    }
    return 1;
}

int aa_stream_write_message(uint8_t channel_id, uint8_t message_type, uint8_t encryption_type,
                            const uint8_t *msg, size_t msg_len,
                            aa_write_fn write, void *ctx) {
    uint8_t buf[AA_MAX_FRAME_PAYLOAD + 8];
    size_t out_len = 0;

    if (msg_len <= AA_MAX_FRAME_PAYLOAD) {
        aa_frame f;
        f.channel_id = channel_id;
        f.frame_type = AA_FRAME_BULK;
        f.message_type = message_type;
        f.encryption_type = encryption_type;
        f.payload = msg;
        f.payload_size = (uint16_t)msg_len;
        f.total_message_size = 0;
        if (aa_frame_encode(&f, buf, sizeof(buf), &out_len) != 0) {
            return -1;
        }
        return write(ctx, buf, out_len);
    }

    aa_frame f;
    f.channel_id = channel_id;
    f.message_type = message_type;
    f.encryption_type = encryption_type;

    f.frame_type = AA_FRAME_FIRST;
    f.payload = msg;
    f.payload_size = AA_MAX_FRAME_PAYLOAD;
    f.total_message_size = (uint32_t)msg_len;
    if (aa_frame_encode(&f, buf, sizeof(buf), &out_len) != 0) {
        return -1;
    }
    if (write(ctx, buf, out_len) != 0) {
        return -1;
    }

    size_t off = AA_MAX_FRAME_PAYLOAD;
    size_t remaining = msg_len - AA_MAX_FRAME_PAYLOAD;
    while (remaining > 0) {
        size_t chunk = remaining > AA_MAX_FRAME_PAYLOAD ? AA_MAX_FRAME_PAYLOAD : remaining;
        f.frame_type = (remaining <= AA_MAX_FRAME_PAYLOAD) ? AA_FRAME_LAST : AA_FRAME_MIDDLE;
        f.payload = msg + off;
        f.payload_size = (uint16_t)chunk;
        f.total_message_size = 0;
        if (aa_frame_encode(&f, buf, sizeof(buf), &out_len) != 0) {
            return -1;
        }
        if (write(ctx, buf, out_len) != 0) {
            return -1;
        }
        off += chunk;
        remaining -= chunk;
    }

    return 0;
}
