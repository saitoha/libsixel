/* SPDX-License-Identifier: MIT */
/* Retained commit execution: admission fallback. */
#include "../../_support/codec/retained_component_common.h"

int
test_retained_admission_fallback(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_component_check(1, 4, 1,
        RETAINED_COMPONENT_ADMISSION);
}
