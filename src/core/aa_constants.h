#ifndef PSVITAAUTO_AA_CONSTANTS_H
#define PSVITAAUTO_AA_CONSTANTS_H

#include <stdint.h>

#define AA_MAX_FRAME_PAYLOAD 0x4000u

/* Channel/service IDs (modern aap_protobuf numbering, per open-headunit). */
typedef enum {
    AA_CHANNEL_CONTROL      = 0,
    AA_CHANNEL_SENSOR       = 1,
    AA_CHANNEL_VIDEO        = 2,
    AA_CHANNEL_INPUT        = 3,
    AA_CHANNEL_SPEECH_AUDIO = 4,
    AA_CHANNEL_SYSTEM_AUDIO = 5,
    AA_CHANNEL_MEDIA_AUDIO  = 6
} aa_channel_id_t;

typedef enum {
    AA_FRAME_MIDDLE = 0x00,
    AA_FRAME_FIRST  = 0x01,
    AA_FRAME_LAST   = 0x02,
    AA_FRAME_BULK   = 0x03
} aa_frame_type_t;

typedef enum {
    AA_MSG_SPECIFIC = 0x00,
    AA_MSG_CONTROL  = 0x04
} aa_message_type_t;

typedef enum {
    AA_ENC_PLAIN    = 0x00,
    AA_ENC_ENCRYPTED = 0x08
} aa_encryption_type_t;

#endif
