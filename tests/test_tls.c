#include "test.h"

#include <string.h>

#include "tls.h"
#include "test_certs.h"

static void test_cert_loads(void) {
    aa_tls_session *c = aa_tls_create(0, NULL, NULL);
    TEST_ASSERT_TRUE(c != NULL);
    aa_tls_destroy(c);
}

static void test_handshake_and_roundtrip(void) {
    aa_tls_session *server = aa_tls_create(1, TEST_CERT_PEM, TEST_KEY_PEM);
    aa_tls_session *client = aa_tls_create(0, NULL, NULL);
    TEST_ASSERT_TRUE(server != NULL);
    TEST_ASSERT_TRUE(client != NULL);

    uint8_t cb[16384], sb[16384];
    size_t cblen = 0, sblen = 0;
    int c_done = 0, s_done = 0;

    int r = aa_tls_handshake(client, NULL, 0, cb, sizeof(cb), &cblen);
    TEST_ASSERT_TRUE(r >= 0);
    if (r == 0) {
        c_done = 1;
    }

    for (int i = 0; i < 200 && !(c_done && s_done); i++) {
        if (!s_done) {
            sblen = 0;
            r = aa_tls_handshake(server, cb, cblen, sb, sizeof(sb), &sblen);
            cblen = 0;
            if (r < 0) {
                TEST_ASSERT_TRUE(0);
                break;
            }
            if (r == 0) {
                s_done = 1;
            }
        }
        if (!c_done) {
            cblen = 0;
            r = aa_tls_handshake(client, sb, sblen, cb, sizeof(cb), &cblen);
            sblen = 0;
            if (r < 0) {
                TEST_ASSERT_TRUE(0);
                break;
            }
            if (r == 0) {
                c_done = 1;
            }
        }
    }
    TEST_ASSERT_TRUE(c_done);
    TEST_ASSERT_TRUE(s_done);

    const char *msg = "hello from client";
    uint8_t ciph[4096];
    size_t clen = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_tls_encrypt(client, (const uint8_t *)msg, strlen(msg),
                                            ciph, sizeof(ciph), &clen));
    uint8_t pt[4096];
    int plen = aa_tls_decrypt(server, ciph, clen, pt, sizeof(pt));
    TEST_ASSERT_EQUAL_INT((int)strlen(msg), plen);
    TEST_ASSERT_TRUE(memcmp(pt, msg, strlen(msg)) == 0);

    const char *msg2 = "hello from server";
    TEST_ASSERT_EQUAL_INT(0, aa_tls_encrypt(server, (const uint8_t *)msg2, strlen(msg2),
                                            ciph, sizeof(ciph), &clen));
    plen = aa_tls_decrypt(client, ciph, clen, pt, sizeof(pt));
    TEST_ASSERT_EQUAL_INT((int)strlen(msg2), plen);
    TEST_ASSERT_TRUE(memcmp(pt, msg2, strlen(msg2)) == 0);

    aa_tls_destroy(client);
    aa_tls_destroy(server);
}

int main(void) {
    TEST_RUN(test_cert_loads);
    TEST_RUN(test_handshake_and_roundtrip);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
