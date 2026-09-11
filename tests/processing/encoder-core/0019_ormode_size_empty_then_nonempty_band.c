/* SPDX-License-Identifier: MIT */
/* Check OR size-policy geometry and its distinct band branch. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"

int
test_or_enc_0019(int argc, char **argv)
{
    unsigned char source[7] = { 0 };

    (void)argc;
    (void)argv;
    source[6] = 1;
    return test_or_encode(source, 1, 7, 4,
                          SIXEL_ENCODEPOLICY_SIZE, 0, 0,
                          "-#1@$");
}
