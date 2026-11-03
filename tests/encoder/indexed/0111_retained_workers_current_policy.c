/* SPDX-License-Identifier: MIT */
/* Retained commit execution: workers current policy. */
#include "../../_support/codec/retained_workers_common.h"

int
test_retained_workers_current_policy(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_workers_check(4, RETAINED_WORKERS_POLICY);
}
