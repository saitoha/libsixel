/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC split bytes independently of CMS output. */
#include "src/cms.h"
#include "tests/loader/gif_icc_test_common.h"

int
test_gifc_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(5, SIXEL_CMS_ENGINE_BUILTIN);
}
