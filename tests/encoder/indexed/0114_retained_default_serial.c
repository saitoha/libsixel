/* SPDX-License-Identifier: MIT */
/* Retained commit execution: default serial. */
#include "../../_support/codec/retained_component_common.h"

int
test_retained_default_serial(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_component_check(0, 0, 1,
        RETAINED_COMPONENT_WORKERS);
}
