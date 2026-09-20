#include "test.h"
#include "core.h"

static void test_core_init_returns_zero(void) {
    TEST_ASSERT_EQUAL_INT(0, core_init());
}

int main(void) {
    TEST_RUN(test_core_init_returns_zero);
    TEST_REPORT();
    return test_failures == 0 ? 0 : 1;
}
