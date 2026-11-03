/* SPDX-License-Identifier: MIT */
/* Retained commit execution: negative budget. */
#include "../../_support/codec/retained_component_common.h"

int
test_retained_negative_budget(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_component_check(0, -1, 1,
        RETAINED_COMPONENT_NEGATIVE);
}
