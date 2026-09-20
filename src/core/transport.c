#include "transport.h"

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int aa_transport_connect(aa_transport *t, const char *host, uint16_t port) {
    t->fd = -1;
    t->rx_bytes = 0;
    t->tx_bytes = 0;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char portstr[8];
    snprintf(portstr, sizeof(portstr), "%u", (unsigned)port);

    struct addrinfo *res = NULL;
    if (getaddrinfo(host, portstr, &hints, &res) != 0) {
        return -1;
    }

    int fd = -1;
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        return -1;
    }
    t->fd = fd;
    return 0;
}

int aa_transport_send(aa_transport *t, const uint8_t *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = send(t->fd, data + off, len - off, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        off += (size_t)n;
    }
    t->tx_bytes += len;
    return 0;
}

int aa_transport_recv(aa_transport *t, uint8_t *buf, size_t len, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = t->fd;
    pfd.events = POLLIN;

    int pr = poll(&pfd, 1, timeout_ms);
    if (pr <= 0) {
        return (pr == 0) ? 0 : -1;
    }

    ssize_t n = recv(t->fd, buf, len, 0);
    if (n == 0) {
        return -2; /* peer closed the connection */
    }
    if (n < 0) {
        if (errno == EINTR) {
            return 0;
        }
        return -1;
    }
    t->rx_bytes += (size_t)n;
    return (int)n;
}

void aa_transport_close(aa_transport *t) {
    if (t->fd >= 0) {
        close(t->fd);
        t->fd = -1;
    }
}
