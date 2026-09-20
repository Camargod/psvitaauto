#include "test.h"

#include "aa_msg.h"
#include "aa_proto_generated/aap_protobuf/service/inputsource/message/InputReport.pb.h"
#include "aa_proto_generated/aap_protobuf/service/inputsource/message/KeyEvent.pb.h"

static void test_touch_roundtrip(void) {
    aap_protobuf_service_inputsource_message_InputReport r =
        aap_protobuf_service_inputsource_message_InputReport_init_zero;
    r.timestamp = 12345;
    r.has_touch_event = true;
    r.touch_event.pointer_data_count = 1;
    r.touch_event.pointer_data[0].x = 640;
    r.touch_event.pointer_data[0].y = 360;
    r.touch_event.pointer_data[0].pointer_id = 0;
    r.touch_event.has_action = true;
    r.touch_event.action = aap_protobuf_service_inputsource_message_PointerAction_ACTION_DOWN;

    uint8_t buf[128];
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_msg_encode(&r, &aap_protobuf_service_inputsource_message_InputReport_msg,
                                           buf, sizeof(buf), &len));

    aap_protobuf_service_inputsource_message_InputReport d =
        aap_protobuf_service_inputsource_message_InputReport_init_zero;
    TEST_ASSERT_EQUAL_INT(0, aa_msg_decode(buf, len, &d, &aap_protobuf_service_inputsource_message_InputReport_msg));
    TEST_ASSERT_EQUAL_INT(12345, (int)d.timestamp);
    TEST_ASSERT_TRUE(d.has_touch_event);
    TEST_ASSERT_EQUAL_INT(640, (int)d.touch_event.pointer_data[0].x);
    TEST_ASSERT_EQUAL_INT(360, (int)d.touch_event.pointer_data[0].y);
    TEST_ASSERT_EQUAL_INT(aap_protobuf_service_inputsource_message_PointerAction_ACTION_DOWN,
                          d.touch_event.action);
}

static void test_key_roundtrip(void) {
    aap_protobuf_service_inputsource_message_InputReport r =
        aap_protobuf_service_inputsource_message_InputReport_init_zero;
    r.timestamp = 999;
    r.has_key_event = true;
    r.key_event.keys_count = 1;
    r.key_event.keys[0].keycode = 23;
    r.key_event.keys[0].down = true;
    r.key_event.keys[0].metastate = 0;

    uint8_t buf[128];
    size_t len = 0;
    TEST_ASSERT_EQUAL_INT(0, aa_msg_encode(&r, &aap_protobuf_service_inputsource_message_InputReport_msg,
                                           buf, sizeof(buf), &len));

    aap_protobuf_service_inputsource_message_InputReport d =
        aap_protobuf_service_inputsource_message_InputReport_init_zero;
    TEST_ASSERT_EQUAL_INT(0, aa_msg_decode(buf, len, &d, &aap_protobuf_service_inputsource_message_InputReport_msg));
    TEST_ASSERT_TRUE(d.has_key_event);
    TEST_ASSERT_EQUAL_INT(23, (int)d.key_event.keys[0].keycode);
    TEST_ASSERT_TRUE(d.key_event.keys[0].down);
}

int main(void) {
    TEST_RUN(test_touch_roundtrip);
    TEST_RUN(test_key_roundtrip);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
