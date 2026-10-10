#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>

static int sFailures = 0;
static int sChecks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        sChecks++;                                                               \
        if (!(cond)) {                                                           \
            sFailures++;                                                         \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                        \
    } while (0)

#define TEST_SUMMARY()                                                                 \
    (printf("%s: %d checks, %d failures\n", __FILE__, sChecks, sFailures), sFailures != 0)

#endif
