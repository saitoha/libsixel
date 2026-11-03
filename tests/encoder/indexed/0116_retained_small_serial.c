/* SPDX-License-Identifier: MIT */
/* Retained commit execution: small serial. */
#include "../../_support/codec/retained_component_common.h"

int
test_retained_small_serial(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_component_check(0, 8, 1,
        RETAINED_COMPONENT_WORKERS);
}
