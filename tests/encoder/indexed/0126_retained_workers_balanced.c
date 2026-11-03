/* SPDX-License-Identifier: MIT */
#include "../../_support/codec/retained_workers_common.h"

int
test_retained_workers_balanced(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return retained_workers_check(4, RETAINED_WORKERS_BALANCED);
}
