/* SPDX-License-Identifier: MIT */
/* Retained commit execution: abort prepared. */
#include "../../_support/codec/retained_component_common.h"

int
test_retained_abort_prepared(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_component_check(1, 4, 4,
        RETAINED_COMPONENT_ABORT);
}
