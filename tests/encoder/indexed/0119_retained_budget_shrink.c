/* SPDX-License-Identifier: MIT */
/* Retained commit execution: budget shrink. */
#include "../../_support/codec/retained_component_common.h"

int
test_retained_budget_shrink(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_component_check(1, 4, 1,
        RETAINED_COMPONENT_SHRINK);
}
