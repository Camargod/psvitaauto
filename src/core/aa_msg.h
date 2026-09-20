#ifndef PSVITAAUTO_AA_MSG_H
#define PSVITAAUTO_AA_MSG_H

#include <stddef.h>
#include <stdint.h>

#include <pb.h>

/* Control channel message IDs (big-endian uint16 on the wire). */
#define AA_MSG_VERSION_REQUEST           0x0001
#define AA_MSG_VERSION_RESPONSE          0x0002
#define AA_MSG_SSL_HANDSHAKE             0x0003
#define AA_MSG_AUTH_COMPLETE             0x0004
#define AA_MSG_SERVICE_DISCOVERY_REQUEST  0x0005
#define AA_MSG_SERVICE_DISCOVERY_RESPONSE 0x0006
#define AA_MSG_CHANNEL_OPEN_REQUEST      0x0007
#define AA_MSG_CHANNEL_OPEN_RESPONSE     0x0008
#define AA_MSG_PING_REQUEST              0x000b
#define AA_MSG_PING_RESPONSE             0x000c
#define AA_MSG_NAV_FOCUS_REQUEST         0x000d
#define AA_MSG_NAV_FOCUS_NOTIFICATION    0x000e
#define AA_MSG_AUDIO_FOCUS_REQUEST       0x0012
#define AA_MSG_AUDIO_FOCUS_NOTIFICATION  0x0013

/* Input channel message IDs. */
#define AA_MSG_INPUT_EVENT_INDICATION    0x8001
#define AA_MSG_BINDING_REQUEST           0x8002
#define AA_MSG_BINDING_RESPONSE          0x8003

/* Sensor channel message IDs. */
#define AA_MSG_SENSOR_REQUEST            0x8001
#define AA_MSG_SENSOR_RESPONSE           0x8002
#define AA_MSG_SENSOR_BATCH              0x8003

/* AV / video channel message IDs (modern aap_protobuf MediaMessageId). */
#define AA_MSG_MEDIA_DATA                0x0000
#define AA_MSG_MEDIA_CODEC_CONFIG        0x0001
#define AA_MSG_SETUP_REQUEST             0x8000
#define AA_MSG_START_INDICATION          0x8001
#define AA_MSG_STOP_INDICATION           0x8002
#define AA_MSG_SETUP_RESPONSE            0x8003
#define AA_MSG_AV_MEDIA_ACK_INDICATION   0x8004
#define AA_MSG_VIDEO_FOCUS_REQUEST       0x8007
#define AA_MSG_VIDEO_FOCUS_INDICATION    0x8008

/* Generic nanopb encode/decode. Returns 0 on success, -1 on error. */
int aa_msg_encode(const void *msg, const pb_msgdesc_t *desc,
                  uint8_t *out, size_t cap, size_t *len);
int aa_msg_decode(const uint8_t *in, size_t len,
                  void *msg, const pb_msgdesc_t *desc);

#endif
