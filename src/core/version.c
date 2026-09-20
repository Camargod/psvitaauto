#include "version.h"

int aa_version_encode_request(uint16_t major, uint16_t minor,
                              uint8_t *out, size_t cap, size_t *len) {
    if (cap < 4) {
        return -1;
    }
    out[0] = (uint8_t)(major >> 8);
    out[1] = (uint8_t)(major & 0xFF);
    out[2] = (uint8_t)(minor >> 8);
    out[3] = (uint8_t)(minor & 0xFF);
    if (len) {
        *len = 4;
    }
    return 0;
}

int aa_version_encode_response(uint16_t major, uint16_t minor, uint16_t status,
                               uint8_t *out, size_t cap, size_t *len) {
    if (cap < 6) {
        return -1;
    }
    out[0] = (uint8_t)(major >> 8);
    out[1] = (uint8_t)(major & 0xFF);
    out[2] = (uint8_t)(minor >> 8);
    out[3] = (uint8_t)(minor & 0xFF);
    out[4] = (uint8_t)(status >> 8);
    out[5] = (uint8_t)(status & 0xFF);
    if (len) {
        *len = 6;
    }
    return 0;
}

int aa_version_decode_response(const uint8_t *in, size_t len,
                               uint16_t *major, uint16_t *minor, uint16_t *status) {
    if (len < 6) {
        return -1;
    }
    *major = ((uint16_t)in[0] << 8) | in[1];
    *minor = ((uint16_t)in[2] << 8) | in[3];
    *status = ((uint16_t)in[4] << 8) | in[5];
    return 0;
}
