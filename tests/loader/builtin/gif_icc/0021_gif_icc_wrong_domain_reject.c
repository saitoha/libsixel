/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC wrong domain independently of CMS output. */
#include "src/cms.h"
#include "tests/loader/gif_icc_test_common.h"

int
test_gifc_0021(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(21, SIXEL_CMS_ENGINE_BUILTIN);
}
