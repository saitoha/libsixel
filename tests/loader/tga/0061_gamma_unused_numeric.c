/* SPDX-License-Identifier: MIT */
/* Verify TGA gamma unused independently of the decoder/CMS. */
#include "src/cms.h"
#include "tests/_support/loader/tga_color_test_common.h"

int
test_tgac_0020(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_color_test(20, SIXEL_CMS_ENGINE_BUILTIN);
}
