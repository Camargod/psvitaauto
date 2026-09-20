#ifndef PSVITAAUTO_VERSION_H
#define PSVITAAUTO_VERSION_H

#include <stddef.h>
#include <stdint.h>

#define AA_VERSION_MATCH    0x0000
#define AA_VERSION_MISMATCH 0xFFFF

int aa_version_encode_request(uint16_t major, uint16_t minor,
                              uint8_t *out, size_t cap, size_t *len);
int aa_version_encode_response(uint16_t major, uint16_t minor, uint16_t status,
                               uint8_t *out, size_t cap, size_t *len);
int aa_version_decode_response(const uint8_t *in, size_t len,
                               uint16_t *major, uint16_t *minor, uint16_t *status);

#endif
