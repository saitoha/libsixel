/* SPDX-License-Identifier: MIT */
/* Retained commit execution: budget two. */
#include "../../_support/codec/retained_component_common.h"

int
test_retained_budget_two(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_component_check(1, 2, 2,
        RETAINED_COMPONENT_WORKERS);
}
