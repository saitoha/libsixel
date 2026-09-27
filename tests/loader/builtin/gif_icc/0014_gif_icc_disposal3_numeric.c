/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC disposal3 independently of CMS output. */
#include "src/cms.h"
#include "tests/loader/gif_icc_test_common.h"

int
test_gifc_0014(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(14, SIXEL_CMS_ENGINE_BUILTIN);
}
