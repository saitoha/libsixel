/* SPDX-License-Identifier: MIT */
/* Keep unpainted OR index zero opaque through this dequantizer. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"

int
test_or_dec_0027(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_or_dequant("k_undither");
}
