/*
 * SPDX-License-Identifier: MIT
 */

#ifndef FILTER_BINNING_STREAM_ALPHA_ZERO_TEST_COMMON_H
#define FILTER_BINNING_STREAM_ALPHA_ZERO_TEST_COMMON_H

#include <stddef.h>

int filter_binning_stream_alpha_zero_run(int alpha_zero_is_transparent,
                                         size_t expected_point_count,
                                         double expected_total_weight,
                                         int expected_red_points);

#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
