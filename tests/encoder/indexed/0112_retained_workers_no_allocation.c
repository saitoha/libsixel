/* SPDX-License-Identifier: MIT */
/* Retained commit execution: workers no allocation. */
#include "../../_support/codec/retained_workers_common.h"

int
test_retained_workers_no_allocation(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_workers_check(4, RETAINED_WORKERS_NO_ALLOCATION);
}
