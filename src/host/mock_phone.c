#include "mock_phone.h"

#include <stdlib.h>
#include <string.h>

#include "aa_constants.h"
#include "aa_msg.h"
#include "session.h"
#include "tls.h"
#include "transport.h"
#include "version.h"
#include "test_certs.h"

#include "aa_proto_generated/aap_protobuf/service/control/message/ServiceDiscoveryRequest.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/ServiceDiscoveryResponse.pb.h"
#include "aa_proto_generated/aap_protobuf/service/Service.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/ChannelOpenRequest.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/shared/message/Setup.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/shared/message/Start.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/sink/message/KeyBindingRequest.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/PingRequest.pb.h"

struct mock_phone {
    aa_transport t;
    aa_tls_session *tls;
    aa_session *session;
    int step;
    mock_phone_result result;
    int done;
};

static void send_version_response(mock_phone *ph) {
    uint8_t resp[6];
    size_t len = 0;
    aa_version_encode_response(1, 7, AA_VERSION_MATCH, resp, sizeof(resp), &len);
    aa_session_send(ph->session, AA_CHANNEL_CONTROL, AA_MSG_VERSION_RESPONSE,
                    AA_MSG_SPECIFIC, AA_ENC_PLAIN, resp, len);
}

static void send_service_discovery_request(mock_phone *ph) {
    aap_protobuf_service_control_message_ServiceDiscoveryRequest req =
        aap_protobuf_service_control_message_ServiceDiscoveryRequest_init_zero;
    strcpy(req.device_name, "MockPhone");
    strcpy(req.device_brand, "Android");
    uint8_t out[256];
    size_t len = 0;
    aa_msg_encode(&req, &aap_protobuf_service_control_message_ServiceDiscoveryRequest_msg,
                  out, sizeof(out), &len);
    aa_session_send(ph->session, AA_CHANNEL_CONTROL, AA_MSG_SERVICE_DISCOVERY_REQUEST,
                    AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static void send_channel_open(mock_phone *ph, uint8_t channel) {
    aap_protobuf_service_control_message_ChannelOpenRequest req =
        aap_protobuf_service_control_message_ChannelOpenRequest_init_zero;
    req.priority = 1;
    req.service_id = channel;
    uint8_t out[64];
    size_t len = 0;
    aa_msg_encode(&req, &aap_protobuf_service_control_message_ChannelOpenRequest_msg,
                  out, sizeof(out), &len);
    aa_session_send(ph->session, channel, AA_MSG_CHANNEL_OPEN_REQUEST,
                    AA_MSG_CONTROL, AA_ENC_ENCRYPTED, out, len);
}

static void send_setup(mock_phone *ph, uint8_t channel) {
    aap_protobuf_service_media_shared_message_Setup req =
        aap_protobuf_service_media_shared_message_Setup_init_zero;
    req.type = aap_protobuf_service_media_shared_message_MediaCodecType_MEDIA_CODEC_VIDEO_H264_BP;
    uint8_t out[32];
    size_t len = 0;
    aa_msg_encode(&req, &aap_protobuf_service_media_shared_message_Setup_msg,
                  out, sizeof(out), &len);
    aa_session_send(ph->session, channel, AA_MSG_SETUP_REQUEST,
                    AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static void send_start(mock_phone *ph, uint8_t channel, int32_t session) {
    aap_protobuf_service_media_shared_message_Start m =
        aap_protobuf_service_media_shared_message_Start_init_zero;
    m.session_id = session;
    m.configuration_index = 0;
    uint8_t out[32];
    size_t len = 0;
    aa_msg_encode(&m, &aap_protobuf_service_media_shared_message_Start_msg,
                  out, sizeof(out), &len);
    aa_session_send(ph->session, channel, AA_MSG_START_INDICATION,
                    AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static void send_media_frame(mock_phone *ph, uint8_t channel, const uint8_t *data, size_t dlen) {
    aa_session_send(ph->session, channel, AA_MSG_MEDIA_DATA,
                    AA_MSG_SPECIFIC, AA_ENC_PLAIN, data, dlen);
}

static void send_binding(mock_phone *ph) {
    aap_protobuf_service_media_sink_message_KeyBindingRequest req =
        aap_protobuf_service_media_sink_message_KeyBindingRequest_init_zero;
    req.keycodes_count = 1;
    req.keycodes[0] = 23; /* DPAD_CENTER */
    uint8_t out[32];
    size_t len = 0;
    aa_msg_encode(&req, &aap_protobuf_service_media_sink_message_KeyBindingRequest_msg,
                  out, sizeof(out), &len);
    aa_session_send(ph->session, AA_CHANNEL_INPUT, AA_MSG_BINDING_REQUEST,
                    AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static void send_ping(mock_phone *ph) {
    aap_protobuf_service_control_message_PingRequest req =
        aap_protobuf_service_control_message_PingRequest_init_zero;
    req.timestamp = 42;
    uint8_t out[32];
    size_t len = 0;
    aa_msg_encode(&req, &aap_protobuf_service_control_message_PingRequest_msg,
                  out, sizeof(out), &len);
    aa_session_send(ph->session, AA_CHANNEL_CONTROL, AA_MSG_PING_REQUEST,
                    AA_MSG_SPECIFIC, AA_ENC_PLAIN, out, len);
}

mock_phone *mock_phone_create(int fd) {
    mock_phone *ph = calloc(1, sizeof(*ph));
    if (!ph) {
        return NULL;
    }
    ph->t.fd = fd;
    ph->tls = aa_tls_create(1, TEST_CERT_PEM, TEST_KEY_PEM);
    if (!ph->tls) {
        free(ph);
        return NULL;
    }
    ph->session = aa_session_create(&ph->t, ph->tls);
    if (!ph->session) {
        aa_tls_destroy(ph->tls);
        free(ph);
        return NULL;
    }
    return ph;
}

void mock_phone_destroy(mock_phone *ph) {
    if (!ph) {
        return;
    }
    aa_session_destroy(ph->session);
    aa_tls_destroy(ph->tls);
    free(ph);
}

int mock_phone_done(const mock_phone *ph) {
    return ph->done;
}

void mock_phone_result_get(const mock_phone *ph, mock_phone_result *r) {
    *r = ph->result;
}

static void phone_process(mock_phone *ph) {
    int r = aa_session_poll(ph->session);
    if (r != 0) {
        return;
    }
    uint8_t ch;
    uint16_t mid;
    uint8_t *p;
    size_t plen;
    if (aa_session_next_message(ph->session, &ch, &mid, &p, &plen) != 0) {
        return;
    }

    switch (ph->step) {
        case 0:
            if (mid == AA_MSG_VERSION_REQUEST) {
                send_version_response(ph);
                ph->step = 1;
            }
            break;
        case 1:
            if (mid == AA_MSG_SSL_HANDSHAKE) {
                uint8_t out[4096];
                size_t ol = 0;
                int hr = aa_tls_handshake(ph->tls, p, plen, out, sizeof(out), &ol);
                if (ol > 0) {
                    aa_session_send(ph->session, AA_CHANNEL_CONTROL, AA_MSG_SSL_HANDSHAKE,
                                    AA_MSG_SPECIFIC, AA_ENC_PLAIN, out, ol);
                }
                if (hr == 0) {
                    ph->step = 2;
                }
            }
            break;
        case 2:
            if (mid == AA_MSG_AUTH_COMPLETE) {
                send_service_discovery_request(ph);
                ph->step = 3;
            }
            break;
        case 3:
            if (mid == AA_MSG_SERVICE_DISCOVERY_RESPONSE) {
                aap_protobuf_service_control_message_ServiceDiscoveryResponse resp =
                    aap_protobuf_service_control_message_ServiceDiscoveryResponse_init_zero;
                if (aa_msg_decode(p, plen, &resp,
                                  &aap_protobuf_service_control_message_ServiceDiscoveryResponse_msg) == 0) {
                    for (size_t i = 0; i < resp.channels_count; i++) {
                        const aap_protobuf_service_Service *svc = &resp.channels[i];
                        if (svc->id == AA_CHANNEL_VIDEO && svc->has_media_sink_service &&
                            svc->media_sink_service.video_configs_count > 0) {
                            ph->result.video_resolution = svc->media_sink_service.video_configs[0].codec_resolution;
                            ph->result.video_fps = svc->media_sink_service.video_configs[0].frame_rate;
                        }
                    }
                }
                send_channel_open(ph, AA_CHANNEL_VIDEO);
                ph->step = 4;
            }
            break;
        case 4:
            if (mid == AA_MSG_CHANNEL_OPEN_RESPONSE && ch == AA_CHANNEL_VIDEO) {
                ph->result.video_open_ok = 1;
                send_channel_open(ph, AA_CHANNEL_INPUT);
                ph->step = 5;
            }
            break;
        case 5:
            if (mid == AA_MSG_CHANNEL_OPEN_RESPONSE && ch == AA_CHANNEL_INPUT) {
                ph->result.input_open_ok = 1;
                send_channel_open(ph, AA_CHANNEL_MEDIA_AUDIO);
                ph->step = 6;
            }
            break;
        case 6:
            if (mid == AA_MSG_CHANNEL_OPEN_RESPONSE && ch == AA_CHANNEL_MEDIA_AUDIO) {
                ph->result.audio_open_ok = 1;
                send_setup(ph, AA_CHANNEL_VIDEO);
                ph->step = 7;
            }
            break;
        case 7:
            if (mid == AA_MSG_SETUP_RESPONSE) {
                ph->result.setup_ok = 1;
            }
            if (mid == AA_MSG_VIDEO_FOCUS_INDICATION) {
                ph->result.focus_ok = 1;
            }
            if (ph->result.setup_ok && ph->result.focus_ok) {
                send_start(ph, AA_CHANNEL_VIDEO, 1);
                ph->step = 8;
            }
            break;
        case 9:
            if (mid == AA_MSG_AV_MEDIA_ACK_INDICATION && ch == AA_CHANNEL_VIDEO) {
                ph->result.video_ack_ok = 1;
                ph->step = 10;
            }
            break;
        case 11:
            if (mid == AA_MSG_AV_MEDIA_ACK_INDICATION && ch == AA_CHANNEL_MEDIA_AUDIO) {
                ph->result.audio_ack_ok = 1;
                ph->step = 12;
            }
            break;
        case 13:
            if (mid == AA_MSG_BINDING_RESPONSE && ch == AA_CHANNEL_INPUT) {
                ph->result.binding_ok = 1;
                ph->step = 14;
            }
            break;
        case 15:
            if (mid == AA_MSG_PING_RESPONSE) {
                ph->result.ping_ok = 1;
                ph->done = 1;
            }
            break;
        default:
            break;
    }
}

static void phone_advance(mock_phone *ph) {
    switch (ph->step) {
        case 8: {
            uint8_t h264[] = {0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84};
            send_media_frame(ph, AA_CHANNEL_VIDEO, h264, sizeof(h264));
            ph->step = 9;
            break;
        }
        case 10: {
            uint8_t pcm[] = {0x11, 0x22, 0x33, 0x44};
            send_media_frame(ph, AA_CHANNEL_MEDIA_AUDIO, pcm, sizeof(pcm));
            ph->step = 11;
            break;
        }
        case 12:
            send_binding(ph);
            ph->step = 13;
            break;
        case 14:
            send_ping(ph);
            ph->step = 15;
            break;
        default:
            break;
    }
}

void mock_phone_step(mock_phone *ph) {
    phone_process(ph);
    phone_advance(ph);
}
