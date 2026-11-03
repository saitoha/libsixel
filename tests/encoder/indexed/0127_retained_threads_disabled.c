/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include "../../_support/codec/retained_component_common.h"

int
test_retained_threads_disabled(int argc, char **argv)
{
    (void)argc;
    (void)argv;
#if SIXEL_ENABLE_THREADS
    return 77;
#else
    return retained_component_check(1, 4, 1, RETAINED_COMPONENT_WORKERS);
#endif
}
