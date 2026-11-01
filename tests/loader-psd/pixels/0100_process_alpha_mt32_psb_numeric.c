/* SPDX-License-Identifier: MIT */
/* Verify process alpha mt32 psb numeric. */
#include "../../_support/loader/builtin/psd_process_test_common.h"

int
test_psd_color_0109(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return psd_process_test(109);
}
