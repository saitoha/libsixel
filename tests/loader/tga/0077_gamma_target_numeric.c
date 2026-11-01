/* SPDX-License-Identifier: MIT */
/* Verify TGA gamma target independently of the decoder/CMS. */
#include "src/cms.h"
#include "tests/_support/loader/tga_color_test_common.h"

int
test_tgac_0036(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_color_test(36, SIXEL_CMS_ENGINE_BUILTIN);
}
