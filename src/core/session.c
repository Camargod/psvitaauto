#include "session.h"

#include <stdlib.h>
#include <string.h>

#include "aa_constants.h"
#include "frame.h"
#include "tls.h"

typedef struct aa_session {
    aa_transport *transport;
    aa_tls_session *tls;

    uint8_t *in;
    size_t in_len;
    size_t in_cap;

    uint8_t *msg;
    size_t msg_len;
    size_t msg_cap;
    uint32_t msg_total;
    uint8_t msg_channel;
    int in_message;

    uint8_t *out_msg;
    size_t out_msg_len;
    uint8_t out_channel;
} aa_session;

aa_session *aa_session_create(aa_transport *t, aa_tls_session *tls) {
    aa_session *s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }
    s->transport = t;
    s->tls = tls;
    return s;
}

void aa_session_destroy(aa_session *s) {
    if (!s) {
        return;
    }
    free(s->in);
    free(s->msg);
    free(s->out_msg);
    free(s);
}

void aa_session_set_tls(aa_session *s, aa_tls_session *tls) {
    if (s) {
        s->tls = tls;
    }
}

static int ensure_cap(uint8_t **buf, size_t *cap, size_t need) {
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

static int send_frame(aa_session *s, uint8_t channel_id, uint8_t message_type,
                      uint8_t encryption, uint8_t frame_type,
                      const uint8_t *plain, size_t plain_len, uint32_t total_size) {
    uint8_t buf[AA_MAX_FRAME_PAYLOAD + 8];
    uint8_t ciph[AA_MAX_FRAME_PAYLOAD + 8];
    const uint8_t *payload = plain;
    uint16_t payload_size = (uint16_t)plain_len;

    if (encryption == AA_ENC_ENCRYPTED && s->tls) {
        size_t clen = 0;
        if (aa_tls_encrypt(s->tls, plain, plain_len, ciph, sizeof(ciph), &clen) != 0) {
            return -1;
        }
        payload = ciph;
        payload_size = (uint16_t)clen;
    }

    aa_frame f;
    f.channel_id = channel_id;
    f.frame_type = frame_type;
    f.message_type = message_type;
    f.encryption_type = encryption;
    f.payload = payload;
    f.payload_size = payload_size;
    f.total_message_size = total_size;

    size_t out_len = 0;
    if (aa_frame_encode(&f, buf, sizeof(buf), &out_len) != 0) {
        return -1;
    }
    return aa_transport_send(s->transport, buf, out_len);
}

int aa_session_send(aa_session *s, uint8_t channel_id, uint16_t message_id,
                    uint8_t message_type, uint8_t encryption,
                    const uint8_t *payload, size_t payload_len) {
    size_t total = 2 + payload_len;
    uint8_t *full = malloc(total);
    if (!full) {
        return -1;
    }
    full[0] = (uint8_t)(message_id >> 8);
    full[1] = (uint8_t)(message_id & 0xFF);
    if (payload_len > 0) {
        memcpy(full + 2, payload, payload_len);
    }

    int result = 0;
    if (total <= AA_MAX_FRAME_PAYLOAD) {
        result = send_frame(s, channel_id, message_type, encryption, AA_FRAME_BULK,
                            full, total, 0);
    } else {
        if (send_frame(s, channel_id, message_type, encryption, AA_FRAME_FIRST,
                       full, AA_MAX_FRAME_PAYLOAD, (uint32_t)total) != 0) {
            result = -1;
        } else {
            size_t off = AA_MAX_FRAME_PAYLOAD;
            size_t remaining = total - AA_MAX_FRAME_PAYLOAD;
            while (remaining > 0 && result == 0) {
                size_t chunk = remaining > AA_MAX_FRAME_PAYLOAD ? AA_MAX_FRAME_PAYLOAD : remaining;
                uint8_t ft = (remaining <= AA_MAX_FRAME_PAYLOAD) ? AA_FRAME_LAST : AA_FRAME_MIDDLE;
                if (send_frame(s, channel_id, message_type, encryption, ft,
                               full + off, chunk, 0) != 0) {
                    result = -1;
                }
                off += chunk;
                remaining -= chunk;
            }
        }
    }

    free(full);
    return result;
}

int aa_session_poll(aa_session *s) {
    uint8_t tmp[2048];
    for (;;) {
        int n = aa_transport_recv(s->transport, tmp, sizeof(tmp), 0);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            break;
        }
        if (ensure_cap(&s->in, &s->in_cap, s->in_len + (size_t)n) != 0) {
            return -1;
        }
        memcpy(s->in + s->in_len, tmp, (size_t)n);
        s->in_len += (size_t)n;
    }

    while (s->in_len > 0) {
        aa_frame f;
        size_t consumed = 0;
        if (aa_frame_decode(s->in, s->in_len, &f, &consumed) != 0) {
            return 1;
        }

        const uint8_t *chunk = f.payload;
        size_t chunk_len = f.payload_size;
        uint8_t *plain_buf = NULL;

        if (f.encryption_type == AA_ENC_ENCRYPTED && s->tls) {
            plain_buf = malloc(f.payload_size + 1);
            if (!plain_buf) {
                return -1;
            }
            int plen = aa_tls_decrypt(s->tls, f.payload, f.payload_size, plain_buf, f.payload_size + 1);
            if (plen < 0) {
                free(plain_buf);
                return -1;
            }
            chunk = plain_buf;
            chunk_len = (size_t)plen;
        }

        if (f.frame_type == AA_FRAME_FIRST) {
            s->msg_total = f.total_message_size;
            s->msg_len = 0;
            s->msg_channel = f.channel_id;
            s->in_message = 1;
        } else if (!s->in_message) {
            s->msg_total = (uint32_t)chunk_len;
            s->msg_len = 0;
            s->msg_channel = f.channel_id;
            s->in_message = 1;
        }

        if (ensure_cap(&s->msg, &s->msg_cap, s->msg_len + chunk_len) != 0) {
            free(plain_buf);
            return -1;
        }
        memcpy(s->msg + s->msg_len, chunk, chunk_len);
        s->msg_len += chunk_len;
        free(plain_buf);

        memmove(s->in, s->in + consumed, s->in_len - consumed);
        s->in_len -= consumed;

        if (s->in_message && s->msg_len >= s->msg_total) {
            s->in_message = 0;
            free(s->out_msg);
            s->out_msg = s->msg;
            s->out_msg_len = s->msg_len;
            s->out_channel = s->msg_channel;
            s->msg = NULL;
            s->msg_len = 0;
            s->msg_cap = 0;
            return 0;
        }
    }
    return 1;
}

int aa_session_next_message(aa_session *s, uint8_t *channel_id, uint16_t *message_id,
                            uint8_t **payload, size_t *payload_len) {
    if (!s->out_msg || s->out_msg_len < 2) {
        return -1;
    }
    if (channel_id) {
        *channel_id = s->out_channel;
    }
    if (message_id) {
        *message_id = ((uint16_t)s->out_msg[0] << 8) | s->out_msg[1];
    }
    *payload = s->out_msg + 2;
    *payload_len = s->out_msg_len - 2;
    return 0;
}
