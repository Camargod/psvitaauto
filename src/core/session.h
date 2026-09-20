#ifndef PSVITAAUTO_SESSION_H
#define PSVITAAUTO_SESSION_H

#include <stddef.h>
#include <stdint.h>

#include "transport.h"

typedef struct aa_session aa_session;
typedef struct aa_tls_session aa_tls_session;

aa_session *aa_session_create(aa_transport *t, aa_tls_session *tls);
void aa_session_destroy(aa_session *s);

/* Update the TLS context used for encrypt/decrypt (deferred TLS setup). */
void aa_session_set_tls(aa_session *s, aa_tls_session *tls);

/* Send a message: 2-byte BE message_id + payload, framed, per-frame encrypted
 * when encryption == AA_ENC_ENCRYPTED. Returns 0 on success, -1 on error. */
int aa_session_send(aa_session *s, uint8_t channel_id, uint16_t message_id,
                    uint8_t message_type, uint8_t encryption,
                    const uint8_t *payload, size_t payload_len);

/* Drain the transport and reassemble one message.
 * Returns 0 when a message is ready, 1 when more data is needed, -1 on error. */
int aa_session_poll(aa_session *s);

/* Return the most recently assembled message (valid until the next poll).
 * Returns 0 on success, -1 if no message is available. */
int aa_session_next_message(aa_session *s, uint8_t *channel_id, uint16_t *message_id,
                            uint8_t **payload, size_t *payload_len);

#endif
