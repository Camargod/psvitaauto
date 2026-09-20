#include "discover.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int get_local_ip(uint32_t *out) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    struct sockaddr_in local;
    socklen_t len = sizeof(local);
    if (getsockname(fd, (struct sockaddr *)&local, &len) != 0) {
        close(fd);
        return -1;
    }
    close(fd);
    *out = ntohl(local.sin_addr.s_addr);
    return 0;
}

int aa_discover_probe(const char *ip, uint16_t port, int timeout_ms) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return 0;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
        close(fd);
        return 0;
    }

    int ok = 0;
    int r = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
    if (r == 0) {
        ok = 1;
    } else if (errno == EINPROGRESS) {
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        if (poll(&pfd, 1, timeout_ms) > 0) {
            int err = 0;
            socklen_t elen = sizeof(err);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &elen);
            ok = (err == 0);
        }
    }
    close(fd);
    return ok;
}

int aa_discover_head_unit(uint16_t port, char *out_ip, size_t cap) {
    uint32_t local = 0;
    if (get_local_ip(&local) != 0) {
        return -1;
    }
    uint32_t base = local & 0xFFFFFF00u;

    for (uint32_t host = 1; host < 255; host++) {
        uint32_t ip = base | host;
        if (ip == local) {
            continue;
        }
        char ipstr[16];
        struct in_addr a;
        a.s_addr = htonl(ip);
        snprintf(ipstr, sizeof(ipstr), "%s", inet_ntoa(a));
        if (aa_discover_probe(ipstr, port, 80)) {
            snprintf(out_ip, cap, "%s", ipstr);
            return 0;
        }
    }
    return -1;
}
