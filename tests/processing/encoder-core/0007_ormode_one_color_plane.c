/* SPDX-License-Identifier: MIT */
/* Lock OR plane selectors at this palette boundary. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"

int
test_or_enc_0007(int argc, char **argv)
{
    unsigned char source[1] = { 0 };

    (void)argc;
    (void)argv;
    return test_or_encode(source, 1, 1, 1,
                          SIXEL_ENCODEPOLICY_FAST, 0, 0,
                          "#1?$");
}
