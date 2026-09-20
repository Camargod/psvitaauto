#include "test.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "discover.h"

static void test_probe_open(void) {
    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    TEST_ASSERT_TRUE(lfd >= 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    addr.sin_port = 0;
    TEST_ASSERT_EQUAL_INT(0, bind(lfd, (struct sockaddr *)&addr, sizeof(addr)));

    socklen_t alen = sizeof(addr);
    TEST_ASSERT_EQUAL_INT(0, getsockname(lfd, (struct sockaddr *)&addr, &alen));
    TEST_ASSERT_EQUAL_INT(0, listen(lfd, 1));

    uint16_t port = ntohs(addr.sin_port);
    TEST_ASSERT_EQUAL_INT(1, aa_discover_probe("127.0.0.1", port, 2000));

    close(lfd);
}

static void test_probe_closed(void) {
    TEST_ASSERT_EQUAL_INT(0, aa_discover_probe("127.0.0.1", 1, 300));
}

int main(void) {
    TEST_RUN(test_probe_open);
    TEST_RUN(test_probe_closed);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
