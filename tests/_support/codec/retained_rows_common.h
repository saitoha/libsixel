/* SPDX-License-Identifier: MIT */
#ifndef TEST_RETAINED_ROWS_COMMON_H
#define TEST_RETAINED_ROWS_COMMON_H

/* Each caller supplies one fixed fixture and one observation. The helper
 * iterates only over pixels, never over backends or parameter combinations. */
enum retained_rows_observation {
    RETAINED_ROWS_RGB,
    RETAINED_ROWS_VALID,
    RETAINED_ROWS_PAINT,
    RETAINED_ROWS_COUNT,
    RETAINED_ROWS_REFINEMENT,
    RETAINED_ROWS_BACKEND
};

int retained_rows_check(int neon, int width, int height, int colors,
    int pattern, int record_paint, int first, int last,
    enum retained_rows_observation observation);

#endif
