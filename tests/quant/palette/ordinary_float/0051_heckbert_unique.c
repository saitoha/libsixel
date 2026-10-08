/* SPDX-License-Identifier: MIT */
/* Heckbert distinguishes subbyte float colors. */
#include "ordinary_float_test_common.h"

int
test_ordinary_heckbert_unique(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return ordinary_heckbert_unique() ? 0 : 1;
}
