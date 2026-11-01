/* SPDX-License-Identifier: MIT */
#ifndef TEST_ORMODE_ENCODE_COMMON_H
#define TEST_ORMODE_ENCODE_COMMON_H

/* Check literal body bytes and independently decode the indexed geometry. */
int test_or_encode(unsigned char const *source, int width, int height,
                   int colors, int policy, int left, int top,
                   char const *expected_body);

int test_or_pipeline(int policy, int left, int top);

#endif
