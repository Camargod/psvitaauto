#include "test.h"

#include "aa_msg.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/PingRequest.pb.h"
#include "aa_proto_generated/aap_protobuf/service/control/message/ChannelOpenRequest.pb.h"

static void test_constants(void) {
    TEST_ASSERT_EQUAL_INT(3, AA_MSG_SSL_HANDSHAKE);
    TEST_ASSERT_EQUAL_INT(1, AA_MSG_VERSION_REQUEST);
    TEST_ASSERT_EQUAL_INT(7, AA_MSG_CHANNEL_OPEN_REQUEST);
    TEST_ASSERT_EQUAL_INT(0xb, AA_MSG_PING_REQUEST);
}

static void test_ping_roundtrip(void) {
    aap_protobuf_service_control_message_PingRequest src =
        aap_protobuf_service_control_message_PingRequest_init_zero;
    src.timestamp = 1234567890;

    uint8_t buf[64];
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_msg_encode(&src, &aap_protobuf_service_control_message_PingRequest_msg,
                                           buf, sizeof(buf), &len));

    aap_protobuf_service_control_message_PingRequest dst =
        aap_protobuf_service_control_message_PingRequest_init_zero;
    TEST_ASSERT_EQUAL_INT(0, aa_msg_decode(buf, len, &dst, &aap_protobuf_service_control_message_PingRequest_msg));
    TEST_ASSERT_EQUAL_INT((int)src.timestamp, (int)dst.timestamp);
}

static void test_channel_open_roundtrip(void) {
    aap_protobuf_service_control_message_ChannelOpenRequest src =
        aap_protobuf_service_control_message_ChannelOpenRequest_init_zero;
    src.priority = 1;
    src.service_id = 3;

    uint8_t buf[64];
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_msg_encode(&src, &aap_protobuf_service_control_message_ChannelOpenRequest_msg,
                                           buf, sizeof(buf), &len));

    aap_protobuf_service_control_message_ChannelOpenRequest dst =
        aap_protobuf_service_control_message_ChannelOpenRequest_init_zero;
    TEST_ASSERT_EQUAL_INT(0, aa_msg_decode(buf, len, &dst, &aap_protobuf_service_control_message_ChannelOpenRequest_msg));
    TEST_ASSERT_EQUAL_INT(src.priority, dst.priority);
    TEST_ASSERT_EQUAL_INT(src.service_id, dst.service_id);
}

int main(void) {
    TEST_RUN(test_constants);
    TEST_RUN(test_ping_roundtrip);
    TEST_RUN(test_channel_open_roundtrip);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
