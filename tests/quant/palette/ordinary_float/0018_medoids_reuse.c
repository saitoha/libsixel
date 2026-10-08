/* SPDX-License-Identifier: MIT */
/* A failed ordinary float build permits a clean subsequent attempt. */
#include "ordinary_float_test_common.h"

int
test_ordinary_medoids_reuse(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_KMEDOIDS, 0, 1) ? 0 : 1;
}
