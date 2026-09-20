#include "test.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "transport.h"

typedef struct {
    int listen_fd;
    int port;
} server_ctx;

static void *echo_server(void *arg) {
    server_ctx *ctx = arg;
    int c = accept(ctx->listen_fd, NULL, NULL);
    if (c < 0) {
        return NULL;
    }
    uint8_t buf[256];
    ssize_t n = recv(c, buf, sizeof(buf), 0);
    if (n > 0) {
        send(c, buf, (size_t)n, 0);
    }
    close(c);
    return NULL;
}

static void test_loopback(void) {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT_TRUE(lfd >= 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    TEST_ASSERT_EQUAL_INT(0, bind(lfd, (struct sockaddr *)&addr, sizeof(addr)));

    socklen_t alen = sizeof(addr);
    TEST_ASSERT_EQUAL_INT(0, getsockname(lfd, (struct sockaddr *)&addr, &alen));
    TEST_ASSERT_EQUAL_INT(0, listen(lfd, 1));

    server_ctx ctx = {lfd, ntohs(addr.sin_port)};
    pthread_t th;
    TEST_ASSERT_EQUAL_INT(0, pthread_create(&th, NULL, echo_server, &ctx));

    aa_transport t;
    TEST_ASSERT_EQUAL_INT(0, aa_transport_connect(&t, "127.0.0.1", (uint16_t)ctx.port));

    uint8_t data[] = "hello aa-transport";
    size_t n = strlen((char *)data);
    TEST_ASSERT_EQUAL_INT(0, aa_transport_send(&t, data, n));

    uint8_t rx[256];
    int got = aa_transport_recv(&t, rx, sizeof(rx), 3000);
    TEST_ASSERT_EQUAL_INT((int)n, got);
    TEST_ASSERT_TRUE(memcmp(rx, data, n) == 0);

    aa_transport_close(&t);
    pthread_join(th, NULL);
    close(lfd);
}

int main(void) {
    TEST_RUN(test_loopback);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
