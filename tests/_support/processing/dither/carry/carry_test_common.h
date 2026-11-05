/* SPDX-License-Identifier: MIT */
#ifndef SIXEL_TEST_PROCESSING_DITHER_CARRY_CARRY_TEST_COMMON_H
#define SIXEL_TEST_PROCESSING_DITHER_CARRY_CARRY_TEST_COMMON_H

int carry_test_numeric(int scenario);
int carry_test_bias(int method);
int carry_test_pipeline(int scenario);
int carry_test_keep(int method, int perturb, int early);
int carry_test_keep_incoming(int early);

#endif /* SIXEL_TEST_PROCESSING_DITHER_CARRY_CARRY_TEST_COMMON_H */
