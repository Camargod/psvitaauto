#ifndef PSVITAAUTO_DISCOVER_H
#define PSVITAAUTO_DISCOVER_H

#include <stddef.h>
#include <stdint.h>

/* Probe a single dotted IPv4 address on a port. Returns 1 if open. */
int aa_discover_probe(const char *ip, uint16_t port, int timeout_ms);

/* Scan the local /24 subnet for an open port; on success writes the dotted
 * IPv4 string of the first hit to out_ip and returns 0. */
int aa_discover_head_unit(uint16_t port, char *out_ip, size_t cap);

#endif
