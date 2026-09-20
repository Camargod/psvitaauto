#ifndef PSVITAAUTO_STREAM_H
#define PSVITAAUTO_STREAM_H

#include <stddef.h>
#include <stdint.h>

typedef int (*aa_write_fn)(void *ctx, const uint8_t *data, size_t len);

int aa_stream_write_message(uint8_t channel_id, uint8_t message_type, uint8_t encryption_type,
                            const uint8_t *msg, size_t msg_len,
                            aa_write_fn write, void *ctx);

typedef struct aa_stream_reader {
    uint8_t *in;
    size_t in_len;
    size_t in_cap;

    uint8_t *msg;
    size_t msg_len;
    size_t msg_cap;
    uint32_t msg_total;
    uint8_t msg_channel;
    uint8_t msg_message_type;
    int in_message;
} aa_stream_reader;

void aa_stream_reader_init(aa_stream_reader *r);
void aa_stream_reader_free(aa_stream_reader *r);
int aa_stream_reader_feed(aa_stream_reader *r, const uint8_t *data, size_t len);
int aa_stream_reader_next_message(aa_stream_reader *r,
                                  uint8_t **out_msg, size_t *out_msg_len,
                                  uint8_t *out_channel, uint8_t *out_message_type);

#endif
