/* SPDX-License-Identifier: MIT */
#ifndef TEST_ORMODE_DECODE_COMMON_H
#define TEST_ORMODE_DECODE_COMMON_H

int test_or_decode(char const *stream, unsigned char const *expected,
                   int width, int height, int threads, int direct);
int test_or_dequant(char const *method);

#endif
