#ifndef PSVITAAUTO_FRAME_H
#define PSVITAAUTO_FRAME_H

#include <stddef.h>
#include <stdint.h>

#include "aa_constants.h"

typedef struct {
    uint8_t channel_id;
    uint8_t frame_type;
    uint8_t message_type;
    uint8_t encryption_type;
    const uint8_t *payload;
    uint16_t payload_size;
    uint32_t total_message_size;
} aa_frame;

int aa_frame_encode(const aa_frame *f, uint8_t *out, size_t out_cap, size_t *out_len);
int aa_frame_decode(const uint8_t *buf, size_t len, aa_frame *out, size_t *consumed);

#endif
