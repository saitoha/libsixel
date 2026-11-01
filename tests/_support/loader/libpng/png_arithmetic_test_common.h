/* SPDX-License-Identifier: MIT */
#ifndef PNG_ARITHMETIC_TEST_COMMON_H
#define PNG_ARITHMETIC_TEST_COMMON_H
int png_arith_dispatch(int sample, int requested);
int png_arith_values(int sample, int requested, int adapt, int exact);
int png_arith_tiny(int sample, int requested);
int png_arith_neutral(int requested);
int png_arith_target(void);
int png_arith_build_atomic(void);
int png_arith_pixel_atomic(int alias);
int png_arith_load(int sample, char const *order, int exact);
#endif
