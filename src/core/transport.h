#ifndef PSVITAAUTO_TRANSPORT_H
#define PSVITAAUTO_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int fd;
    size_t rx_bytes;
    size_t tx_bytes;
} aa_transport;

int aa_transport_connect(aa_transport *t, const char *host, uint16_t port);
int aa_transport_send(aa_transport *t, const uint8_t *data, size_t len);
int aa_transport_recv(aa_transport *t, uint8_t *buf, size_t len, int timeout_ms);
void aa_transport_close(aa_transport *t);

#endif
