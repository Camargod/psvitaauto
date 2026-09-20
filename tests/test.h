#ifndef PSVITAAUTO_TEST_H
#define PSVITAAUTO_TEST_H

#include <stdio.h>

static int test_checks = 0;
static int test_failures = 0;

#define TEST_ASSERT_EQUAL_INT(expected, actual)                                   \
    do {                                                                          \
        test_checks++;                                                            \
        if ((expected) != (actual)) {                                             \
            test_failures++;                                                      \
            fprintf(stderr, "FAIL %s:%d: expected %d, got %d\n",                 \
                    __FILE__, __LINE__, (int)(expected), (int)(actual));          \
        }                                                                         \
    } while (0)

#define TEST_ASSERT_TRUE(cond)                                                    \
    do {                                                                          \
        test_checks++;                                                            \
        if (!(cond)) {                                                            \
            test_failures++;                                                      \
            fprintf(stderr, "FAIL %s:%d: condition false\n", __FILE__, __LINE__); \
        }                                                                         \
    } while (0)

#define TEST_RUN(fn)               \
    do {                           \
        printf("RUN %s\n", #fn);   \
        (fn)();                    \
    } while (0)

#define TEST_REPORT()                                                     \
    do {                                                                  \
        printf("%d checks, %d failures\n", test_checks, test_failures);   \
        if (test_failures != 0) {                                         \
            fprintf(stderr, "FAILED\n");                                  \
        } else {                                                          \
            printf("ALL PASSED\n");                                       \
        }                                                                 \
    } while (0)

#endif
