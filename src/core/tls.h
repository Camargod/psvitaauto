#ifndef PSVITAAUTO_TLS_H
#define PSVITAAUTO_TLS_H

#include <stddef.h>
#include <stdint.h>

typedef struct aa_tls_session aa_tls_session;

/*
 * is_server: 0 = client (AA head unit role), 1 = server (harness/tests).
 * cert_pem/key_pem NULL + client => embedded AA certificate.
 * Returns NULL on failure.
 */
aa_tls_session *aa_tls_create(int is_server, const char *cert_pem, const char *key_pem);
void aa_tls_destroy(aa_tls_session *s);

/* Diagnostic info from the last aa_tls_create() attempt.
 * step: 1=ctr_drbg_seed 2=x509_crt_parse 3=pk_parse_key
 *       4=ssl_config_defaults 5=ssl_setup. */
int aa_tls_last_step(void);
int aa_tls_last_rc(void);

/* 0 = handshake complete, 1 = WANT_READ (need more inbound bytes), -1 = error */
int aa_tls_handshake(aa_tls_session *s,
                     const uint8_t *in, size_t in_len,
                     uint8_t *out, size_t out_cap, size_t *out_len);

/* 0 on success (out_len set), -1 on error */
int aa_tls_encrypt(aa_tls_session *s, const uint8_t *plain, size_t plain_len,
                   uint8_t *out, size_t out_cap, size_t *out_len);

/* returns plaintext length (>=0), -1 error, -2 WANT_READ */
int aa_tls_decrypt(aa_tls_session *s, const uint8_t *cipher, size_t cipher_len,
                   uint8_t *out, size_t out_cap);

#endif
