/* SPDX-License-Identifier: MIT */
/* Float allocation failure cannot retry byte or another solver. */
#include "ordinary_float_test_common.h"

int
test_ordinary_medoids_fault(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_failure(SIXEL_QUANTIZE_MODEL_KMEDOIDS, 0, 0) ? 0 : 1;
}
