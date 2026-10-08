/* SPDX-License-Identifier: MIT */
/* Invalid float coordinates fail without a lossy retry. */
#include "ordinary_float_test_common.h"

int
test_ordinary_medoids_nonfinite(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_KMEDOIDS, 1, 0) ? 0 : 1;
}
