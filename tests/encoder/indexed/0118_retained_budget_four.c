/* SPDX-License-Identifier: MIT */
/* Retained commit execution: budget four. */
#include "../../_support/codec/retained_component_common.h"

int
test_retained_budget_four(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_component_check(1, 4, 4,
        RETAINED_COMPONENT_WORKERS);
}
