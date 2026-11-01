/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_MULTI_PASS_TEST_COMMON_H
#define SIXEL_MULTI_PASS_TEST_COMMON_H
#include <stddef.h>
int test_multi_pass_capture(char const *stream, size_t size,
    unsigned char const *expected, int width, int height, size_t passes,
    char const *mode);
#endif
