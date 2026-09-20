#include "frame.h"

#include <string.h>

int aa_frame_encode(const aa_frame *f, uint8_t *out, size_t out_cap, size_t *out_len) {
    int extended = (f->frame_type == AA_FRAME_FIRST);
    size_t header_len = 2 + (extended ? 6 : 2);
    size_t total = header_len + f->payload_size;

    if (out_cap < total) {
        return -1;
    }

    out[0] = f->channel_id;
    out[1] = (uint8_t)(f->frame_type | f->message_type | f->encryption_type);

    size_t off = 2;
    if (extended) {
        out[off++] = (uint8_t)(f->payload_size >> 8);
        out[off++] = (uint8_t)(f->payload_size & 0xFF);
        out[off++] = (uint8_t)(f->total_message_size >> 24);
        out[off++] = (uint8_t)(f->total_message_size >> 16);
        out[off++] = (uint8_t)(f->total_message_size >> 8);
        out[off++] = (uint8_t)(f->total_message_size & 0xFF);
    } else {
        out[off++] = (uint8_t)(f->payload_size >> 8);
        out[off++] = (uint8_t)(f->payload_size & 0xFF);
    }

    if (f->payload_size > 0) {
        memcpy(out + off, f->payload, f->payload_size);
    }

    if (out_len) {
        *out_len = total;
    }
    return 0;
}

int aa_frame_decode(const uint8_t *buf, size_t len, aa_frame *out, size_t *consumed) {
    if (len < 4) {
        return -1;
    }

    out->channel_id = buf[0];
    out->frame_type = buf[1] & 0x03;
    out->message_type = buf[1] & 0x04;
    out->encryption_type = buf[1] & 0x08;

    uint32_t payload_size = 0;
    uint32_t total_message_size = 0;
    size_t off = 2;

    if (out->frame_type == AA_FRAME_FIRST) {
        if (len < 8) {
            return -1;
        }
        payload_size = ((uint32_t)buf[off] << 8) | buf[off + 1];
        off += 2;
        total_message_size = ((uint32_t)buf[off] << 24) | ((uint32_t)buf[off + 1] << 16) |
                             ((uint32_t)buf[off + 2] << 8) | buf[off + 3];
        off += 4;
    } else {
        payload_size = ((uint32_t)buf[off] << 8) | buf[off + 1];
        off += 2;
    }

    if (off + payload_size > len) {
        return -1;
    }

    out->payload = buf + off;
    out->payload_size = (uint16_t)payload_size;
    out->total_message_size = total_message_size;

    if (consumed) {
        *consumed = off + payload_size;
    }
    return 0;
}
