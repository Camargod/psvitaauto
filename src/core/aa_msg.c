#include "aa_msg.h"

#include <pb_decode.h>
#include <pb_encode.h>

int aa_msg_encode(const void *msg, const pb_msgdesc_t *desc,
                  uint8_t *out, size_t cap, size_t *len) {
    pb_ostream_t stream = pb_ostream_from_buffer(out, cap);
    if (!pb_encode(&stream, desc, msg)) {
        return -1;
    }
    if (len) {
        *len = stream.bytes_written;
    }
    return 0;
}

int aa_msg_decode(const uint8_t *in, size_t len,
                  void *msg, const pb_msgdesc_t *desc) {
    pb_istream_t stream = pb_istream_from_buffer(in, len);
    if (!pb_decode(&stream, desc, msg)) {
        return -1;
    }
    return 0;
}
