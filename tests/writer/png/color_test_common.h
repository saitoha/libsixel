/* SPDX-License-Identifier: MIT */
#ifndef PNG_COLOR_TEST_COMMON_H
#define PNG_COLOR_TEST_COMMON_H
int png_color_check(int expected_space, int direct, int sample_case);
int png_color_write(char const *path, int mode);
int png_color_raw(void);
int png_color_profile(void);
#endif
