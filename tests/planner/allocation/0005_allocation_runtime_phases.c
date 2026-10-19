/* SPDX-License-Identifier: MIT */
#include <stddef.h>
#include "src/planner.h"
#include "allocation_runtime_common.h"

int
test_allocation_runtime_phases(int argc, char **argv)
{
    static allocation_runtime_expected_t const expected[] = {
        {1, 1, 1, 0, 0, SIXEL_DITHER_ENCODE_SERIAL, 1,
            SIXEL_DIFFUSE_FS, NULL},
        {2, 2, 2, 48, 6, SIXEL_DITHER_ENCODE_STAGED, 2,
            SIXEL_DIFFUSE_FS, NULL},
        {3, 3, 3, 36, 6, SIXEL_DITHER_ENCODE_STAGED, 3,
            SIXEL_DIFFUSE_FS, NULL},
        {4, 4, 4, 24, 6, SIXEL_DITHER_ENCODE_STAGED, 4,
            SIXEL_DIFFUSE_FS, NULL},
        {6, 4, 2, 24, 6, SIXEL_DITHER_ENCODE_PIPELINE, 4,
            SIXEL_DIFFUSE_FS, NULL},
        {8, 6, 2, 18, 6, SIXEL_DITHER_ENCODE_PIPELINE, 6,
            SIXEL_DIFFUSE_FS, NULL},
        {10, 7, 3, 18, 6, SIXEL_DITHER_ENCODE_PIPELINE, 6,
            SIXEL_DIFFUSE_FS, NULL},
        {14, 10, 4, 12, 6, SIXEL_DITHER_ENCODE_PIPELINE, 8,
            SIXEL_DIFFUSE_FS, NULL},
        {2, 2, 2, 12, 3, SIXEL_DITHER_ENCODE_STAGED, 8,
            SIXEL_DIFFUSE_FS, "fs:band_width=9:band_overwrap=3"}
    };

    if (argc != 2) {
        return 1;
    }
    return allocation_runtime_cases(expected,
        (int)(sizeof(expected) / sizeof(expected[0])), 0, argv[1]);
}
