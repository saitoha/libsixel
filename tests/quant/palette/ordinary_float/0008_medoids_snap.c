/* SPDX-License-Identifier: MIT */
/* Preservation retains the explicitly requested final partial snap. */
#include "ordinary_float_test_common.h"

int
test_ordinary_medoids_snap(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_snap(SIXEL_QUANTIZE_MODEL_KMEDOIDS) ? 0 : 1;
}
