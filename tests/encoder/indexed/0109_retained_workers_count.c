/* SPDX-License-Identifier: MIT */
/* Retained commit execution: workers count. */
#include "../../_support/codec/retained_workers_common.h"

int
test_retained_workers_count(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_workers_check(4, RETAINED_WORKERS_COUNT);
}
