/* SPDX-License-Identifier: MIT */
/* Verify TGA short raw reject independently of the decoder/CMS. */
#include "src/cms.h"
#include "tests/_support/loader/tga_color_test_common.h"

int
test_tgac_0029(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_color_test(29, SIXEL_CMS_ENGINE_BUILTIN);
}
