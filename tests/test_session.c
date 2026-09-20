#include "test.h"

#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "aa_constants.h"
#include "aa_msg.h"
#include "session.h"
#include "tls.h"
#include "transport.h"
#include "version.h"
#include "test_certs.h"

static void make_pair(aa_transport *a, aa_transport *b) {
    int fds[2];
    socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    a->fd = fds[0];
    b->fd = fds[1];
}

static void test_version_codec(void) {
    uint8_t req[4];
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_version_encode_request(1, 1, req, sizeof(req), &len));
    TEST_ASSERT_EQUAL_INT(4, (int)len);
    TEST_ASSERT_TRUE(req[0] == 0 && req[1] == 1 && req[2] == 0 && req[3] == 1);

    uint8_t resp[6];
    TEST_ASSERT_EQUAL_INT(0, aa_version_encode_response(1, 1, AA_VERSION_MATCH, resp, sizeof(resp), &len));
    uint16_t maj = 0, min = 0, status = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_version_decode_response(resp, len, &maj, &min, &status));
    TEST_ASSERT_EQUAL_INT(1, maj);
    TEST_ASSERT_EQUAL_INT(1, min);
    TEST_ASSERT_EQUAL_INT(AA_VERSION_MATCH, status);
}

static void test_plain_roundtrip(void) {
    aa_transport a, b;
    make_pair(&a, &b);
    aa_session *sa = aa_session_create(&a, NULL);
    aa_session *sb = aa_session_create(&b, NULL);
    TEST_ASSERT_TRUE(sa != NULL && sb != NULL);

    const uint8_t payload[] = {0xde, 0xad, 0xbe, 0xef};
    TEST_ASSERT_EQUAL_INT(0, aa_session_send(sa, AA_CHANNEL_CONTROL, AA_MSG_PING_REQUEST,
                                             AA_MSG_SPECIFIC, AA_ENC_PLAIN,
                                             payload, sizeof(payload)));

    uint8_t ch = 0;
    uint16_t mid = 0;
    uint8_t *p = NULL;
    size_t plen = 0;
    int got = 0;
    for (int i = 0; i < 10 && !got; i++) {
        if (aa_session_poll(sb) == 0) {
            got = 1;
        }
    }
    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_EQUAL_INT(0, aa_session_next_message(sb, &ch, &mid, &p, &plen));
    TEST_ASSERT_EQUAL_INT(AA_CHANNEL_CONTROL, ch);
    TEST_ASSERT_EQUAL_INT(AA_MSG_PING_REQUEST, mid);
    TEST_ASSERT_EQUAL_INT(sizeof(payload), (int)plen);
    TEST_ASSERT_TRUE(memcmp(p, payload, sizeof(payload)) == 0);

    aa_session_destroy(sa);
    aa_session_destroy(sb);
    close(a.fd);
    close(b.fd);
}

static void tls_handshake(aa_tls_session *server, aa_tls_session *client) {
    uint8_t cb[16384], sb[16384];
    size_t cblen = 0, sblen = 0;
    int c_done = 0, s_done = 0;
    int r = aa_tls_handshake(client, NULL, 0, cb, sizeof(cb), &cblen);
    if (r == 0) {
        c_done = 1;
    }
    for (int i = 0; i < 200 && !(c_done && s_done); i++) {
        if (!s_done) {
            sblen = 0;
            r = aa_tls_handshake(server, cb, cblen, sb, sizeof(sb), &sblen);
            cblen = 0;
            if (r == 0) {
                s_done = 1;
            }
        }
        if (!c_done) {
            cblen = 0;
            r = aa_tls_handshake(client, sb, sblen, cb, sizeof(cb), &cblen);
            sblen = 0;
            if (r == 0) {
                c_done = 1;
            }
        }
    }
    TEST_ASSERT_TRUE(c_done && s_done);
}

static void test_encrypted_roundtrip(void) {
    aa_transport a, b;
    make_pair(&a, &b);
    aa_tls_session *server = aa_tls_create(1, TEST_CERT_PEM, TEST_KEY_PEM);
    aa_tls_session *client = aa_tls_create(0, NULL, NULL);
    TEST_ASSERT_TRUE(server != NULL && client != NULL);
    tls_handshake(server, client);

    aa_session *sa = aa_session_create(&a, client);
    aa_session *sb = aa_session_create(&b, server);
    TEST_ASSERT_TRUE(sa != NULL && sb != NULL);

    const uint8_t payload[] = {1, 2, 3, 4, 5};
    TEST_ASSERT_EQUAL_INT(0, aa_session_send(sa, AA_CHANNEL_CONTROL, AA_MSG_SERVICE_DISCOVERY_REQUEST,
                                             AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED,
                                             payload, sizeof(payload)));

    uint8_t ch = 0;
    uint16_t mid = 0;
    uint8_t *p = NULL;
    size_t plen = 0;
    int got = 0;
    for (int i = 0; i < 10 && !got; i++) {
        if (aa_session_poll(sb) == 0) {
            got = 1;
        }
    }
    TEST_ASSERT_TRUE(got);
    TEST_ASSERT_EQUAL_INT(0, aa_session_next_message(sb, &ch, &mid, &p, &plen));
    TEST_ASSERT_EQUAL_INT(AA_MSG_SERVICE_DISCOVERY_REQUEST, mid);
    TEST_ASSERT_EQUAL_INT(sizeof(payload), (int)plen);
    TEST_ASSERT_TRUE(memcmp(p, payload, sizeof(payload)) == 0);

    aa_session_destroy(sa);
    aa_session_destroy(sb);
    aa_tls_destroy(client);
    aa_tls_destroy(server);
    close(a.fd);
    close(b.fd);
}

int main(void) {
    TEST_RUN(test_version_codec);
    TEST_RUN(test_plain_roundtrip);
    TEST_RUN(test_encrypted_roundtrip);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
