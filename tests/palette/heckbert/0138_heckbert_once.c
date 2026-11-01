/* SPDX-License-Identifier: MIT */
/* Heckbert once snap runs only on the final palette. */
#include "../../_support/quant/palette/ordinary_float/ordinary_float_test_common.h"

int
test_ordinary_heckbert_once(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_heckbert_once() ? 0 : 1;
}
