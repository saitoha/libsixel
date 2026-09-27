/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC replay independently of CMS output. */
#include "src/cms.h"
#include "tests/loader/gif_icc_test_common.h"

int
test_gifc_0018(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(18, SIXEL_CMS_ENGINE_BUILTIN);
}
