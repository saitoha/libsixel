/* SPDX-License-Identifier: MIT */
/* Verify TGA large gamma reject independently of the decoder/CMS. */
#include "src/cms.h"
#include "tests/loader/tga_color_test_common.h"

int
test_tgac_0023(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_color_test(23, SIXEL_CMS_ENGINE_BUILTIN);
}
