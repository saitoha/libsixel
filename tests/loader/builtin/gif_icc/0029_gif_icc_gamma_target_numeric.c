/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC gamma target independently of CMS output. */
#include "src/cms.h"
#include "tests/loader/gif_icc_test_common.h"

int
test_gifc_0029(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(29, SIXEL_CMS_ENGINE_BUILTIN);
}
