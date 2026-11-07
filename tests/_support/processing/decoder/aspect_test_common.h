/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_ASPECT_TEST_COMMON_H
#define SIXEL_ASPECT_TEST_COMMON_H
/* R/G/B/K are literal colors; a dot denotes an unpainted sample. */
int test_decoder_aspect_pixels(char const *stream, int width, int height,
    char const *expected, unsigned int options, unsigned int expected_flags);
#define ASPECT_COLORS "#1;2;100;0;0#2;2;0;100;0#3;2;0;0;100"
#define ASPECT_PAINT "#1@?$#2?A"
#endif
