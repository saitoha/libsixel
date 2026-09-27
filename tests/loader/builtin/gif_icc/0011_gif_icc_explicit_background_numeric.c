/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC explicit background independently of CMS output. */
#include "src/cms.h"
#include "tests/loader/gif_icc_test_common.h"

int
test_gifc_0011(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(11, SIXEL_CMS_ENGINE_BUILTIN);
}
