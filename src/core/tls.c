#include "tls.h"

#include <stdlib.h>
#include <string.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/pk.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"

#include "aa_cert.h"

#if defined(__vita__)
#include <psp2/kernel/rng.h>
#include <psp2/kernel/processmgr.h>
#include <debugnet.h>
static int vita_entropy_func(void *ctx, unsigned char *output, size_t len, size_t *olen) {
    (void)ctx;
    size_t total = 0;
    while (total < len) {
        size_t chunk = len - total;
        if (chunk > 64) {
            chunk = 64;
        }
        int r = sceKernelGetRandomNumber(output + total, chunk);
        if (r != 0) {
            debugNetPrintf(ERROR, "PSVitaAuto: sceKernelGetRandomNumber(%zu)=0x%08X\n", chunk, r);
            return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
        }
        total += chunk;
    }
    *olen = total;
    return 0;
}
mbedtls_ms_time_t mbedtls_ms_time(void) {
    return (mbedtls_ms_time_t)(sceKernelGetProcessTimeWide() / 1000);
}
#endif

static int g_step = 0;
static int g_rc = 0;

int aa_tls_last_step(void) { return g_step; }
int aa_tls_last_rc(void) { return g_rc; }

typedef struct bio_ctx {
    uint8_t *in;
    size_t in_len;
    size_t in_off;
    size_t in_cap;
    uint8_t *out;
    size_t out_len;
    size_t out_cap;
} bio_ctx;

struct aa_tls_session {
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cert;
    mbedtls_pk_context key;
    mbedtls_ctr_drbg_context drbg;
    mbedtls_entropy_context entropy;
    bio_ctx bio;
};

static int bio_send(void *ctx, const unsigned char *buf, size_t len) {
    bio_ctx *b = ctx;
    if (b->out_len + len > b->out_cap) {
        size_t ncap = b->out_cap ? b->out_cap : 4096;
        while (ncap < b->out_len + len) {
            ncap *= 2;
        }
        uint8_t *nb = realloc(b->out, ncap);
        if (!nb) {
            return MBEDTLS_ERR_SSL_ALLOC_FAILED;
        }
        b->out = nb;
        b->out_cap = ncap;
    }
    memcpy(b->out + b->out_len, buf, len);
    b->out_len += len;
    return (int)len;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len) {
    bio_ctx *b = ctx;
    if (b->in_off >= b->in_len) {
        return MBEDTLS_ERR_SSL_WANT_READ;
    }
    size_t avail = b->in_len - b->in_off;
    size_t n = avail < len ? avail : len;
    memcpy(buf, b->in + b->in_off, n);
    b->in_off += n;
    return (int)n;
}

static int bio_reserve(uint8_t **buf, size_t *cap, size_t need) {
    if (need <= *cap) {
        return 0;
    }
    size_t ncap = *cap ? *cap : 256;
    while (ncap < need) {
        ncap *= 2;
    }
    uint8_t *nb = realloc(*buf, ncap);
    if (!nb) {
        return -1;
    }
    *buf = nb;
    *cap = ncap;
    return 0;
}

static void bio_compact(bio_ctx *b) {
    if (b->in_off > 0) {
        if (b->in_off < b->in_len) {
            memmove(b->in, b->in + b->in_off, b->in_len - b->in_off);
        }
        b->in_len -= b->in_off;
        b->in_off = 0;
    }
}

aa_tls_session *aa_tls_create(int is_server, const char *cert_pem, const char *key_pem) {
    aa_tls_session *s = calloc(1, sizeof(*s));
    if (!s) {
        return NULL;
    }

    if (cert_pem == NULL && !is_server) {
        cert_pem = aa_cert_pem();
        key_pem = aa_key_pem();
    }
    if (cert_pem == NULL || key_pem == NULL) {
        aa_tls_destroy(s);
        return NULL;
    }

    mbedtls_ssl_init(&s->ssl);
    mbedtls_ssl_config_init(&s->conf);
    mbedtls_x509_crt_init(&s->cert);
    mbedtls_pk_init(&s->key);
    mbedtls_entropy_init(&s->entropy);
    mbedtls_ctr_drbg_init(&s->drbg);

#if defined(__vita__)
    mbedtls_entropy_add_source(&s->entropy, vita_entropy_func, NULL, 32,
                               MBEDTLS_ENTROPY_SOURCE_STRONG);
#endif

    const unsigned char *pers = (const unsigned char *)"psvitaauto";
    g_step = 1;
    int drbg_r = mbedtls_ctr_drbg_seed(&s->drbg, mbedtls_entropy_func, &s->entropy, pers, 10);
    g_rc = drbg_r;
    if (drbg_r != 0) {
#if defined(__vita__)
        debugNetPrintf(ERROR, "PSVitaAuto: ctr_drbg_seed failed 0x%04X\n", drbg_r);
#endif
        aa_tls_destroy(s);
        return NULL;
    }
    g_step = 2;
    int crt_r = mbedtls_x509_crt_parse(&s->cert, (const unsigned char *)cert_pem, strlen(cert_pem) + 1);
    g_rc = crt_r;
    if (crt_r != 0) {
#if defined(__vita__)
        debugNetPrintf(ERROR, "PSVitaAuto: x509_crt_parse failed 0x%04X\n", crt_r);
#endif
        aa_tls_destroy(s);
        return NULL;
    }
    g_step = 3;
    int key_r = mbedtls_pk_parse_key(&s->key, (const unsigned char *)key_pem, strlen(key_pem) + 1,
                                     NULL, 0, NULL, NULL);
    g_rc = key_r;
    if (key_r != 0) {
#if defined(__vita__)
        debugNetPrintf(ERROR, "PSVitaAuto: pk_parse_key failed 0x%04X\n", key_r);
#endif
        aa_tls_destroy(s);
        return NULL;
    }

    int endpoint = is_server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;
    g_step = 4;
    int cfg_r = mbedtls_ssl_config_defaults(&s->conf, endpoint,
                                            MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT);
    g_rc = cfg_r;
    if (cfg_r != 0) {
#if defined(__vita__)
        debugNetPrintf(ERROR, "PSVitaAuto: ssl_config_defaults failed 0x%04X\n", cfg_r);
#endif
        aa_tls_destroy(s);
        return NULL;
    }

    mbedtls_ssl_conf_rng(&s->conf, mbedtls_ctr_drbg_random, &s->drbg);
    mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_min_tls_version(&s->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&s->conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_own_cert(&s->conf, &s->cert, &s->key);

    g_step = 5;
    int setup_r = mbedtls_ssl_setup(&s->ssl, &s->conf);
    g_rc = setup_r;
    if (setup_r != 0) {
#if defined(__vita__)
        debugNetPrintf(ERROR, "PSVitaAuto: ssl_setup failed 0x%04X\n", setup_r);
#endif
        aa_tls_destroy(s);
        return NULL;
    }
    mbedtls_ssl_set_bio(&s->ssl, &s->bio, bio_send, bio_recv, NULL);

    return s;
}

void aa_tls_destroy(aa_tls_session *s) {
    if (!s) {
        return;
    }
    mbedtls_ssl_free(&s->ssl);
    mbedtls_ssl_config_free(&s->conf);
    mbedtls_x509_crt_free(&s->cert);
    mbedtls_pk_free(&s->key);
    mbedtls_ctr_drbg_free(&s->drbg);
    mbedtls_entropy_free(&s->entropy);
    free(s->bio.in);
    free(s->bio.out);
    free(s);
}

int aa_tls_handshake(aa_tls_session *s,
                     const uint8_t *in, size_t in_len,
                     uint8_t *out, size_t out_cap, size_t *out_len) {
    bio_ctx *b = &s->bio;

    if (in_len > 0) {
        bio_compact(b);
        if (bio_reserve(&b->in, &b->in_cap, b->in_len + in_len) != 0) {
            return -1;
        }
        memcpy(b->in + b->in_len, in, in_len);
        b->in_len += in_len;
    }

    b->out_len = 0;

    int ret;
    int guard = 0;
    do {
        ret = mbedtls_ssl_handshake(&s->ssl);
        if (++guard > 64) {
            return -1;
        }
    } while (ret == MBEDTLS_ERR_SSL_WANT_WRITE);

    int result;
    if (ret == 0) {
        result = 0;
    } else if (ret == MBEDTLS_ERR_SSL_WANT_READ) {
        result = 1;
    } else {
        result = -1;
    }

    if (b->out_len > out_cap) {
        return -1;
    }
    if (b->out_len > 0) {
        memcpy(out, b->out, b->out_len);
    }
    if (out_len) {
        *out_len = b->out_len;
    }
    b->out_len = 0;

    return result;
}

int aa_tls_encrypt(aa_tls_session *s, const uint8_t *plain, size_t plain_len,
                   uint8_t *out, size_t out_cap, size_t *out_len) {
    bio_ctx *b = &s->bio;
    b->out_len = 0;

    size_t off = 0;
    while (off < plain_len) {
        int n = mbedtls_ssl_write(&s->ssl, plain + off, plain_len - off);
        if (n <= 0) {
            return -1;
        }
        off += (size_t)n;
    }

    if (b->out_len > out_cap) {
        return -1;
    }
    if (b->out_len > 0) {
        memcpy(out, b->out, b->out_len);
    }
    if (out_len) {
        *out_len = b->out_len;
    }
    b->out_len = 0;
    return 0;
}

int aa_tls_decrypt(aa_tls_session *s, const uint8_t *cipher, size_t cipher_len,
                   uint8_t *out, size_t out_cap) {
    bio_ctx *b = &s->bio;

    bio_compact(b);
    if (bio_reserve(&b->in, &b->in_cap, b->in_len + cipher_len) != 0) {
        return -1;
    }
    memcpy(b->in + b->in_len, cipher, cipher_len);
    b->in_len += cipher_len;

    int ret = mbedtls_ssl_read(&s->ssl, out, out_cap);
    if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return -2;
    }
    if (ret < 0) {
        return -1;
    }
    return ret;
}
