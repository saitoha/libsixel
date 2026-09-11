/* SPDX-License-Identifier: MIT */
/* Compare the fast OR pipeline with its serial body. */
#include <sixel.h>
#include "ormode_test_common.h"

int
test_encoder_core_0005_ormode_pipeline_body_match(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return test_or_pipeline(SIXEL_ENCODEPOLICY_FAST, 0, 0);
}
