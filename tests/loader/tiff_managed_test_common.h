/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_TIFF_MANAGED_TEST_COMMON_H
#define SIXEL_TIFF_MANAGED_TEST_COMMON_H
#include "src/frame-private.h"
int tiff_managed_check(sixel_frame_t const *, int, int, int);
int tiff_managed_values(int, char **, int, int, int);
int tiff_managed_failure(int, char **, int, int, int);
#endif
