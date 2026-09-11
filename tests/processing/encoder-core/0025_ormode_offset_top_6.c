/* SPDX-License-Identifier: MIT */
/* Check every decoded index across this vertical offset boundary. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"

int
test_or_enc_0025(int argc, char **argv)
{
    unsigned char source[4] = { 1, 2, 3, 1 };

    (void)argc;
    (void)argv;
    return test_or_encode(source, 2, 2, 4,
                          SIXEL_ENCODEPOLICY_SIZE, 3, 6, NULL);
}
