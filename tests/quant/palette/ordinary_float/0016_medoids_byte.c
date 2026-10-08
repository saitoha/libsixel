/* SPDX-License-Identifier: MIT */
/* True byte inputs retain byte storage and exact representatives. */
#include "ordinary_float_test_common.h"

int
test_ordinary_medoids_byte(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_float_byte(SIXEL_QUANTIZE_MODEL_KMEDOIDS) ? 0 : 1;
}
