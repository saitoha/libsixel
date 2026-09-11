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
test_or_enc_0018(int argc, char **argv)
{
    unsigned char source[6] = { 1, 0, 1, 0, 1, 0 };

    (void)argc;
    (void)argv;

    return test_or_encode(source, 2, 3, 4,
                          SIXEL_ENCODEPOLICY_SIZE, 0, 0,
                          "#1F?$");
}
