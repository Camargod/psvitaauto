#include "control.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aa_constants.h"
#include "aa_msg.h"
#include "session.h"
#include "tls.h"
#include "version.h"

#if defined(__vita__)
#include <debugnet.h>
#endif

#include "aa_proto_generated/aap_protobuf/service/control/message/AuthResponse.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/ServiceDiscoveryResponse.pb.h"
#include "aa_proto_generated/aap_protobuf/service/Service.pb.h"
#include "aa_proto_generated/aap_protobuf/service/sensorsource/SensorSourceService.pb.h"
#include "aa_proto_generated/aap_protobuf/service/sensorsource/message/Sensor.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/sink/MediaSinkService.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/sink/message/VideoConfiguration.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/shared/message/AudioConfiguration.pb.h"
#include "aa_proto_generated/aap_protobuf/service/inputsource/InputSourceService.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/ChannelOpenResponse.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/PingRequest.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/PingResponse.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/shared/message/Config.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/shared/message/Start.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/video/message/VideoFocusNotification.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/source/message/Ack.pb.h"
#include "aa_proto_generated/aap_protobuf/service/media/sink/message/KeyBindingResponse.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/AudioFocusNotification.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/AudioFocusRequest.pb.h"
#include "aa_proto_generated/aap_protobuf/service/sensorsource/message/SensorResponse.pb.h"
#include "aa_proto_generated/aap_protobuf/service/sensorsource/message/SensorBatch.pb.h"
#include "aa_proto_generated/aap_protobuf/service/inputsource/message/InputReport.pb.h"
#include "aa_proto_generated/aap_protobuf/service/inputsource/message/TouchEvent.pb.h"
#include "aa_proto_generated/aap_protobuf/service/inputsource/message/KeyEvent.pb.h"

typedef enum {
    ST_SEND_VERSION,
    ST_WAIT_VERSION,
    ST_TLS,
    ST_WAIT_SERVICE_DISCOVERY,
    ST_ACTIVE,
    ST_FAILED
} control_stage;

struct aa_control {
    aa_transport *transport;
    aa_session *session;
    aa_tls_session *tls;
    control_stage stage;
    aa_control_state state;

    aa_video_frame_cb video_cb;
    void *video_cb_ctx;
    aa_video_resolution video_resolution;
    aa_video_fps video_fps;
    int log_enabled;
    int32_t session_by_channel[16];
    int input_bound;
};

static void ctl_log(aa_control *c, const char *fmt, ...) {
    if (!c->log_enabled) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stderr);
#if defined(__vita__)
    debugNetPrintf(DEBUG, "AA: %s", buf);
#endif
}

static int send_version_request(aa_control *c) {
    uint8_t buf[4];
    size_t len = 0;
    if (aa_version_encode_request(1, 7, buf, sizeof(buf), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, AA_CHANNEL_CONTROL, AA_MSG_VERSION_REQUEST,
                           AA_MSG_SPECIFIC, AA_ENC_PLAIN, buf, len);
}

static int drive_tls(aa_control *c, const uint8_t *in, size_t in_len) {
    if (c->tls == NULL) {
        c->tls = aa_tls_create(0, NULL, NULL);
        if (!c->tls) {
            return -1;
        }
        aa_session_set_tls(c->session, c->tls);
    }
    uint8_t out[4096];
    size_t out_len = 0;
    int r = aa_tls_handshake(c->tls, in, in_len, out, sizeof(out), &out_len);
    if (out_len > 0) {
        if (aa_session_send(c->session, AA_CHANNEL_CONTROL, AA_MSG_SSL_HANDSHAKE,
                            AA_MSG_SPECIFIC, AA_ENC_PLAIN, out, out_len) != 0) {
            return -1;
        }
    }
    return r;
}

static int send_auth_complete(aa_control *c) {
    aap_protobuf_service_control_message_AuthResponse m =
        aap_protobuf_service_control_message_AuthResponse_init_zero;
    m.status = aap_protobuf_shared_MessageStatus_STATUS_SUCCESS;

    uint8_t out[16];
    size_t len = 0;
    if (aa_msg_encode(&m, &aap_protobuf_service_control_message_AuthResponse_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, AA_CHANNEL_CONTROL, AA_MSG_AUTH_COMPLETE,
                           AA_MSG_SPECIFIC, AA_ENC_PLAIN, out, len);
}

static void add_audio_channel(aap_protobuf_service_Service *s, int32_t id,
                              aap_protobuf_service_media_sink_message_AudioStreamType audio_type,
                              uint32_t rate, uint32_t bits, uint32_t channels) {
    memset(s, 0, sizeof(*s));
    s->id = id;
    s->has_media_sink_service = true;
    s->media_sink_service.has_available_type = true;
    s->media_sink_service.available_type = aap_protobuf_service_media_shared_message_MediaCodecType_MEDIA_CODEC_AUDIO_PCM;
    s->media_sink_service.has_audio_type = true;
    s->media_sink_service.audio_type = audio_type;
    s->media_sink_service.audio_configs_count = 1;
    s->media_sink_service.audio_configs[0].sampling_rate = rate;
    s->media_sink_service.audio_configs[0].number_of_bits = bits;
    s->media_sink_service.audio_configs[0].number_of_channels = channels;
    s->media_sink_service.has_available_while_in_call = true;
    s->media_sink_service.available_while_in_call = true;
}

static int send_service_discovery_response(aa_control *c) {
    aap_protobuf_service_control_message_ServiceDiscoveryResponse resp =
        aap_protobuf_service_control_message_ServiceDiscoveryResponse_init_zero;

    aap_protobuf_service_Service *s;

    s = &resp.channels[resp.channels_count++];
    memset(s, 0, sizeof(*s));
    s->id = AA_CHANNEL_SENSOR;
    s->has_sensor_source_service = true;
    s->sensor_source_service.sensors_count = 2;
    s->sensor_source_service.sensors[0].sensor_type = aap_protobuf_service_sensorsource_message_SensorType_SENSOR_NIGHT_MODE;
    s->sensor_source_service.sensors[1].sensor_type = aap_protobuf_service_sensorsource_message_SensorType_SENSOR_DRIVING_STATUS_DATA;

    s = &resp.channels[resp.channels_count++];
    memset(s, 0, sizeof(*s));
    s->id = AA_CHANNEL_VIDEO;
    s->has_media_sink_service = true;
    s->media_sink_service.has_available_type = true;
    s->media_sink_service.available_type = aap_protobuf_service_media_shared_message_MediaCodecType_MEDIA_CODEC_VIDEO_H264_BP;
    s->media_sink_service.video_configs_count = 1;
    s->media_sink_service.video_configs[0].has_codec_resolution = true;
    s->media_sink_service.video_configs[0].codec_resolution =
        (aap_protobuf_service_media_sink_message_VideoCodecResolutionType)c->video_resolution;
    s->media_sink_service.video_configs[0].has_frame_rate = true;
    s->media_sink_service.video_configs[0].frame_rate =
        (aap_protobuf_service_media_sink_message_VideoFrameRateType)c->video_fps;
    s->media_sink_service.video_configs[0].has_density = true;
    s->media_sink_service.video_configs[0].density = 240;
    s->media_sink_service.video_configs[0].has_width_margin = true;
    s->media_sink_service.video_configs[0].width_margin = 0;
    s->media_sink_service.video_configs[0].has_height_margin = true;
    s->media_sink_service.video_configs[0].height_margin = 0;
    s->media_sink_service.video_configs[0].has_pixel_aspect_ratio_e4 = true;
    s->media_sink_service.video_configs[0].pixel_aspect_ratio_e4 = 10000;
    s->media_sink_service.video_configs[0].has_video_codec_type = true;
    s->media_sink_service.video_configs[0].video_codec_type =
        aap_protobuf_service_media_shared_message_MediaCodecType_MEDIA_CODEC_VIDEO_H264_BP;
    s->media_sink_service.has_available_while_in_call = true;
    s->media_sink_service.available_while_in_call = true;

    add_audio_channel(&resp.channels[resp.channels_count++], AA_CHANNEL_MEDIA_AUDIO,
                      aap_protobuf_service_media_sink_message_AudioStreamType_AUDIO_STREAM_MEDIA,
                      48000, 16, 2);
    add_audio_channel(&resp.channels[resp.channels_count++], AA_CHANNEL_SPEECH_AUDIO,
                      aap_protobuf_service_media_sink_message_AudioStreamType_AUDIO_STREAM_GUIDANCE,
                      16000, 16, 1);
    add_audio_channel(&resp.channels[resp.channels_count++], AA_CHANNEL_SYSTEM_AUDIO,
                      aap_protobuf_service_media_sink_message_AudioStreamType_AUDIO_STREAM_SYSTEM_AUDIO,
                      16000, 16, 1);

    s = &resp.channels[resp.channels_count++];
    memset(s, 0, sizeof(*s));
    s->id = AA_CHANNEL_INPUT;
    s->has_input_source_service = true;
    s->input_source_service.touchscreen_count = 1;
    s->input_source_service.touchscreen[0].width = 1280;
    s->input_source_service.touchscreen[0].height = 720;

    resp.has_make = true;
    strcpy(resp.make, "PSVitaAuto");
    resp.has_model = true;
    strcpy(resp.model, "Vita");
    resp.has_year = true;
    strcpy(resp.year, "2011");
    resp.has_vehicle_id = true;
    strcpy(resp.vehicle_id, "PSVITA001");
    resp.has_driver_position = true;
    resp.driver_position = aap_protobuf_service_control_message_DriverPosition_DRIVER_POSITION_LEFT;
    resp.has_head_unit_make = true;
    strcpy(resp.head_unit_make, "Sony");
    resp.has_head_unit_model = true;
    strcpy(resp.head_unit_model, "PSVita");
    resp.has_head_unit_software_build = true;
    strcpy(resp.head_unit_software_build, "1");
    resp.has_head_unit_software_version = true;
    strcpy(resp.head_unit_software_version, "0.1.0");
    resp.has_can_play_native_media_during_vr = true;
    resp.can_play_native_media_during_vr = false;

    resp.has_headunit_info = true;
    strcpy(resp.headunit_info.make, "PSVitaAuto");
    strcpy(resp.headunit_info.model, "Vita");
    strcpy(resp.headunit_info.year, "2011");
    strcpy(resp.headunit_info.vehicle_id, "PSVITA001");
    strcpy(resp.headunit_info.head_unit_make, "Sony");
    strcpy(resp.headunit_info.head_unit_model, "PSVita");
    strcpy(resp.headunit_info.head_unit_software_build, "1");
    strcpy(resp.headunit_info.head_unit_software_version, "0.1.0");
    resp.headunit_info.has_vehicle_type = true;
    resp.headunit_info.vehicle_type = 3;

    uint8_t out[2048];
    size_t len = 0;
    if (aa_msg_encode(&resp, &aap_protobuf_service_control_message_ServiceDiscoveryResponse_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    ctl_log(c, "sent SERVICE_DISCOVERY_RESPONSE (%zu bytes, %u channels)", len,
         (unsigned)resp.channels_count);
    return aa_session_send(c->session, AA_CHANNEL_CONTROL, AA_MSG_SERVICE_DISCOVERY_RESPONSE,
                           AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static int send_channel_open_response(aa_control *c, uint8_t channel_id) {
    aap_protobuf_service_control_message_ChannelOpenResponse m =
        aap_protobuf_service_control_message_ChannelOpenResponse_init_zero;
    m.status = aap_protobuf_shared_MessageStatus_STATUS_SUCCESS;

    uint8_t out[16];
    size_t len = 0;
    if (aa_msg_encode(&m, &aap_protobuf_service_control_message_ChannelOpenResponse_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, channel_id, AA_MSG_CHANNEL_OPEN_RESPONSE,
                           AA_MSG_CONTROL, AA_ENC_ENCRYPTED, out, len);
}

static int send_setup_response(aa_control *c, uint8_t channel_id) {
    aap_protobuf_service_media_shared_message_Config m =
        aap_protobuf_service_media_shared_message_Config_init_zero;
    m.status = aap_protobuf_service_media_shared_message_Config_Status_STATUS_READY;
    m.has_max_unacked = true;
    m.max_unacked = 1;
    m.configuration_indices_count = 1;
    m.configuration_indices[0] = 0;

    uint8_t out[64];
    size_t len = 0;
    if (aa_msg_encode(&m, &aap_protobuf_service_media_shared_message_Config_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, channel_id, AA_MSG_SETUP_RESPONSE,
                           AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static int send_video_focus(aa_control *c) {
    aap_protobuf_service_media_video_message_VideoFocusNotification m =
        aap_protobuf_service_media_video_message_VideoFocusNotification_init_zero;
    m.has_focus = true;
    m.focus = aap_protobuf_service_media_video_message_VideoFocusMode_VIDEO_FOCUS_PROJECTED;
    m.has_unsolicited = true;
    m.unsolicited = false;

    uint8_t out[16];
    size_t len = 0;
    if (aa_msg_encode(&m, &aap_protobuf_service_media_video_message_VideoFocusNotification_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, AA_CHANNEL_VIDEO, AA_MSG_VIDEO_FOCUS_INDICATION,
                           AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static void handle_start(aa_control *c, uint8_t ch, const uint8_t *p, size_t plen) {
    aap_protobuf_service_media_shared_message_Start m =
        aap_protobuf_service_media_shared_message_Start_init_zero;
    if (aa_msg_decode(p, plen, &m, &aap_protobuf_service_media_shared_message_Start_msg) == 0) {
        if (ch < 16) {
            c->session_by_channel[ch] = m.session_id;
        }
    }
}

static void handle_media_frame(aa_control *c, uint8_t ch, const uint8_t *p, size_t plen) {
    if (ch == AA_CHANNEL_VIDEO && c->video_cb) {
        c->video_cb(c->video_cb_ctx, 0, p, plen);
    }

    aap_protobuf_service_media_source_message_Ack ack =
        aap_protobuf_service_media_source_message_Ack_init_zero;
    ack.session_id = (ch < 16) ? c->session_by_channel[ch] : -1;
    ack.has_ack = true;
    ack.ack = 1;

    uint8_t out[32];
    size_t len = 0;
    if (aa_msg_encode(&ack, &aap_protobuf_service_media_source_message_Ack_msg,
                      out, sizeof(out), &len) == 0) {
        aa_session_send(c->session, ch, AA_MSG_AV_MEDIA_ACK_INDICATION,
                        AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
    }
}

static int send_audio_focus(aa_control *c, aap_protobuf_service_control_message_AudioFocusRequestType type) {
    aap_protobuf_service_control_message_AudioFocusNotification m =
        aap_protobuf_service_control_message_AudioFocusNotification_init_zero;
    if (type == aap_protobuf_service_control_message_AudioFocusRequestType_AUDIO_FOCUS_RELEASE) {
        m.focus_state = aap_protobuf_service_control_message_AudioFocusStateType_AUDIO_FOCUS_STATE_LOSS;
    } else {
        m.focus_state = aap_protobuf_service_control_message_AudioFocusStateType_AUDIO_FOCUS_STATE_GAIN;
    }

    uint8_t out[16];
    size_t len = 0;
    if (aa_msg_encode(&m, &aap_protobuf_service_control_message_AudioFocusNotification_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    ctl_log(c, "sent AUDIO_FOCUS_NOTIFICATION (state=%d)", m.focus_state);
    int r = aa_session_send(c->session, AA_CHANNEL_CONTROL, AA_MSG_AUDIO_FOCUS_NOTIFICATION,
                            AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
    ctl_log(c, "audio focus send result=%d", r);
    return r;
}

static int send_binding_response(aa_control *c, uint8_t ch) {
    aap_protobuf_service_media_sink_message_KeyBindingResponse m =
        aap_protobuf_service_media_sink_message_KeyBindingResponse_init_zero;
    m.status = aap_protobuf_shared_MessageStatus_STATUS_SUCCESS;

    uint8_t out[16];
    size_t len = 0;
    if (aa_msg_encode(&m, &aap_protobuf_service_media_sink_message_KeyBindingResponse_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, ch, AA_MSG_BINDING_RESPONSE,
                           AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static int send_sensor_response(aa_control *c) {
    aap_protobuf_service_sensorsource_message_SensorResponse m =
        aap_protobuf_service_sensorsource_message_SensorResponse_init_zero;
    m.status = aap_protobuf_shared_MessageStatus_STATUS_SUCCESS;

    uint8_t out[16];
    size_t len = 0;
    if (aa_msg_encode(&m, &aap_protobuf_service_sensorsource_message_SensorResponse_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, AA_CHANNEL_SENSOR, AA_MSG_SENSOR_RESPONSE,
                           AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static int send_driving_status(aa_control *c) {
    aap_protobuf_service_sensorsource_message_SensorBatch m =
        aap_protobuf_service_sensorsource_message_SensorBatch_init_zero;
    m.driving_status_data_count = 1;
    m.driving_status_data[0].status = 0;

    uint8_t out[32];
    size_t len = 0;
    if (aa_msg_encode(&m, &aap_protobuf_service_sensorsource_message_SensorBatch_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, AA_CHANNEL_SENSOR, AA_MSG_SENSOR_BATCH,
                           AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static void set_failed(aa_control *c) {
    c->stage = ST_FAILED;
    c->state = AA_CONTROL_FAILED;
}

aa_control *aa_control_create(aa_transport *t) {
    aa_control *c = calloc(1, sizeof(*c));
    if (!c) {
        return NULL;
    }
    c->transport = t;
    c->tls = NULL;
    c->session = aa_session_create(t, NULL);
    if (!c->session) {
        free(c);
        return NULL;
    }
    for (int i = 0; i < 16; i++) {
        c->session_by_channel[i] = -1;
    }
    c->video_resolution = AA_VIDEO_720P;
    c->video_fps = AA_FPS_30;
    c->stage = ST_SEND_VERSION;
    c->state = AA_CONTROL_IDLE;
    return c;
}

void aa_control_destroy(aa_control *c) {
    if (!c) {
        return;
    }
    aa_session_destroy(c->session);
    aa_tls_destroy(c->tls);
    free(c);
}

aa_control_state aa_control_get_state(const aa_control *c) {
    return c->state;
}

int aa_control_get_stage(const aa_control *c) {
    return (int)c->stage;
}

void aa_control_set_video_callback(aa_control *c, aa_video_frame_cb cb, void *ctx) {
    c->video_cb = cb;
    c->video_cb_ctx = ctx;
}

void aa_control_set_video_config(aa_control *c, aa_video_resolution res, aa_video_fps fps) {
    c->video_resolution = res;
    c->video_fps = fps;
}

void aa_control_set_log(aa_control *c, int enabled) {
    c->log_enabled = enabled;
}

int aa_control_send_touch(aa_control *c, uint64_t ts_us, uint32_t x, uint32_t y,
                          uint8_t action, uint32_t pointer_id) {
    aap_protobuf_service_inputsource_message_InputReport r =
        aap_protobuf_service_inputsource_message_InputReport_init_zero;
    r.timestamp = ts_us;
    r.has_touch_event = true;
    r.touch_event.pointer_data_count = 1;
    r.touch_event.pointer_data[0].x = x;
    r.touch_event.pointer_data[0].y = y;
    r.touch_event.pointer_data[0].pointer_id = pointer_id;
    r.touch_event.has_action = true;
    r.touch_event.action = (aap_protobuf_service_inputsource_message_PointerAction)action;

    uint8_t out[128];
    size_t len = 0;
    if (aa_msg_encode(&r, &aap_protobuf_service_inputsource_message_InputReport_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, AA_CHANNEL_INPUT, AA_MSG_INPUT_EVENT_INDICATION,
                           AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

int aa_control_send_key(aa_control *c, uint64_t ts_us, uint32_t keycode, int down) {
    aap_protobuf_service_inputsource_message_InputReport r =
        aap_protobuf_service_inputsource_message_InputReport_init_zero;
    r.timestamp = ts_us;
    r.has_key_event = true;
    r.key_event.keys_count = 1;
    r.key_event.keys[0].keycode = keycode;
    r.key_event.keys[0].down = down ? true : false;
    r.key_event.keys[0].metastate = 0;

    uint8_t out[128];
    size_t len = 0;
    if (aa_msg_encode(&r, &aap_protobuf_service_inputsource_message_InputReport_msg,
                      out, sizeof(out), &len) != 0) {
        return -1;
    }
    return aa_session_send(c->session, AA_CHANNEL_INPUT, AA_MSG_INPUT_EVENT_INDICATION,
                           AA_MSG_SPECIFIC, AA_ENC_ENCRYPTED, out, len);
}

static void dispatch(aa_control *c, uint8_t ch, uint16_t mid,
                     const uint8_t *p, size_t plen) {
    if (mid == AA_MSG_CHANNEL_OPEN_REQUEST) {
        send_channel_open_response(c, ch);
        if (ch == AA_CHANNEL_SENSOR) {
            send_driving_status(c);
        }
        return;
    }

    if (ch == AA_CHANNEL_CONTROL) {
        if (mid == AA_MSG_PING_REQUEST) {
            aap_protobuf_service_control_message_PingRequest req =
                aap_protobuf_service_control_message_PingRequest_init_zero;
            if (aa_msg_decode(p, plen, &req, &aap_protobuf_service_control_message_PingRequest_msg) == 0) {
                aap_protobuf_service_control_message_PingResponse resp =
                    aap_protobuf_service_control_message_PingResponse_init_zero;
                resp.timestamp = req.timestamp;
                uint8_t out[32];
                size_t out_len = 0;
                if (aa_msg_encode(&resp, &aap_protobuf_service_control_message_PingResponse_msg,
                                  out, sizeof(out), &out_len) == 0) {
                    aa_session_send(c->session, AA_CHANNEL_CONTROL, AA_MSG_PING_RESPONSE,
                                    AA_MSG_SPECIFIC, AA_ENC_PLAIN, out, out_len);
                }
            }
        } else if (mid == AA_MSG_AUDIO_FOCUS_REQUEST) {
            aap_protobuf_service_control_message_AudioFocusRequest req =
                aap_protobuf_service_control_message_AudioFocusRequest_init_zero;
            if (aa_msg_decode(p, plen, &req, &aap_protobuf_service_control_message_AudioFocusRequest_msg) == 0) {
                ctl_log(c, "audio focus request type=%d", req.audio_focus_type);
                send_audio_focus(c, req.audio_focus_type);
            }
        }
        return;
    }

    if (ch == AA_CHANNEL_INPUT) {
        if (mid == AA_MSG_BINDING_REQUEST) {
            send_binding_response(c, ch);
            c->input_bound = 1;
        }
        return;
    }

    if (ch == AA_CHANNEL_SENSOR) {
        if (mid == AA_MSG_SENSOR_REQUEST) {
            send_sensor_response(c);
        }
        return;
    }

    switch (mid) {
        case AA_MSG_SETUP_REQUEST:
            if (send_setup_response(c, ch) == 0 && ch == AA_CHANNEL_VIDEO) {
                send_video_focus(c);
            }
            break;
        case AA_MSG_VIDEO_FOCUS_REQUEST:
            send_video_focus(c);
            break;
        case AA_MSG_START_INDICATION:
            handle_start(c, ch, p, plen);
            break;
        case AA_MSG_STOP_INDICATION:
            if (ch < 16) {
                c->session_by_channel[ch] = -1;
            }
            break;
        case AA_MSG_MEDIA_DATA:
        case AA_MSG_MEDIA_CODEC_CONFIG:
            handle_media_frame(c, ch, p, plen);
            break;
        default:
            break;
    }
}

aa_control_state aa_control_poll(aa_control *c) {
    if (c->stage == ST_SEND_VERSION) {
        if (send_version_request(c) != 0) {
            set_failed(c);
            return c->state;
        }
        ctl_log(c, "sent VERSION_REQUEST");
        c->stage = ST_WAIT_VERSION;
    }

    int r = aa_session_poll(c->session);
    if (r == -1) {
        set_failed(c);
        return c->state;
    }
    if (r == 1) {
        return c->state;
    }

    uint8_t ch;
    uint16_t mid;
    uint8_t *p;
    size_t plen;
    if (aa_session_next_message(c->session, &ch, &mid, &p, &plen) != 0) {
        return c->state;
    }

    ctl_log(c, "[stage %d] recv ch=%u mid=0x%04x len=%zu", c->stage, ch, mid, plen);

    switch (c->stage) {
        case ST_WAIT_VERSION:
            if (mid == AA_MSG_VERSION_RESPONSE) {
                uint16_t maj, min, status;
                if (aa_version_decode_response(p, plen, &maj, &min, &status) != 0 ||
                    status != AA_VERSION_MATCH) {
                    ctl_log(c, "VERSION_RESPONSE mismatch/error (maj=%u min=%u status=0x%04x)", maj, min, status);
                    set_failed(c);
                    return c->state;
                }
                ctl_log(c, "received VERSION_RESPONSE (maj=%u min=%u status=MATCH)", maj, min);
                if (drive_tls(c, NULL, 0) < 0) {
                    set_failed(c);
                    return c->state;
                }
                c->stage = ST_TLS;
            }
            break;

        case ST_TLS:
            if (mid == AA_MSG_SSL_HANDSHAKE) {
                int hr = drive_tls(c, p, plen);
                if (hr < 0) {
                    set_failed(c);
                    return c->state;
                }
                if (hr == 0) {
                    ctl_log(c, "TLS handshake complete");
                    if (send_auth_complete(c) != 0) {
                        set_failed(c);
                        return c->state;
                    }
                    c->stage = ST_WAIT_SERVICE_DISCOVERY;
                }
            }
            break;

        case ST_WAIT_SERVICE_DISCOVERY:
            if (mid == AA_MSG_SERVICE_DISCOVERY_REQUEST) {
                if (send_service_discovery_response(c) != 0) {
                    set_failed(c);
                    return c->state;
                }
                c->stage = ST_ACTIVE;
                c->state = AA_CONTROL_ACTIVE;
                ctl_log(c, "session ACTIVE");
            }
            break;

        case ST_ACTIVE:
            dispatch(c, ch, mid, p, plen);
            break;

        default:
            break;
    }

    return c->state;
}
