/* SPDX-License-Identifier: MIT */
/* Compare this OR pipeline policy with its serial body. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"

int
test_or_enc_0028(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_or_pipeline(SIXEL_ENCODEPOLICY_SIZE, 0, 0);
}
