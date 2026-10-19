/* SPDX-License-Identifier: MIT */
#include <stddef.h>
#include "src/planner.h"
#include "allocation_runtime_common.h"

int
test_allocation_effective_none(int argc, char **argv)
{
    static allocation_runtime_expected_t const expected[] = {
        {1, 1, 1, 0, 0, SIXEL_DITHER_ENCODE_SERIAL, 1,
            SIXEL_DIFFUSE_NONE, NULL},
        {2, 1, 2, 0, 0, SIXEL_DITHER_ENCODE_SERIAL, 1,
            SIXEL_DIFFUSE_NONE, NULL},
        {3, 1, 2, 96, 0, SIXEL_DITHER_ENCODE_PIPELINE, 1,
            SIXEL_DIFFUSE_NONE, NULL},
        {4, 2, 2, 48, 0, SIXEL_DITHER_ENCODE_PIPELINE, 2,
            SIXEL_DIFFUSE_NONE, NULL},
        {6, 4, 2, 24, 0, SIXEL_DITHER_ENCODE_PIPELINE, 4,
            SIXEL_DIFFUSE_NONE, NULL},
        {8, 6, 2, 18, 0, SIXEL_DITHER_ENCODE_PIPELINE, 6,
            SIXEL_DIFFUSE_NONE, NULL},
        {10, 7, 3, 18, 0, SIXEL_DITHER_ENCODE_PIPELINE, 6,
            SIXEL_DIFFUSE_NONE, NULL},
        {14, 10, 4, 12, 0, SIXEL_DITHER_ENCODE_PIPELINE, 8,
            SIXEL_DIFFUSE_NONE, NULL}
    };

    if (argc != 2) {
        return 1;
    }
    return allocation_runtime_cases(expected,
        (int)(sizeof(expected) / sizeof(expected[0])), 1, argv[1]);
}
