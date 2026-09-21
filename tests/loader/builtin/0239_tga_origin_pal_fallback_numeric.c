/* SPDX-License-Identifier: MIT */
/* Release the failed PAL8 attempt and preserve RGB index fallback. */
#include "tga_origin_test_common.h"

int
test_loader_0239_tga_origin(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_origin_test(11);
}
