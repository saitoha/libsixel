/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_TIFF_BYTE_TEST_COMMON_H
#define SIXEL_TIFF_BYTE_TEST_COMMON_H
#include "src/frame-private.h"
int tiff_byte_check(sixel_frame_t const *, int, int, int);
int tiff_byte_test(int, char **, int, int, int);
int tiff_byte_faults(int, char **, int);
int tiff_byte_decode(int, char **, int);
#endif
