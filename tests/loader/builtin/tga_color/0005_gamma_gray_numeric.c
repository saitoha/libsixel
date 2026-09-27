/* SPDX-License-Identifier: MIT */
/* Verify TGA gamma gray independently of the decoder/CMS. */
#include "src/cms.h"
#include "tests/loader/tga_color_test_common.h"

int
test_tgac_0005(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_color_test(5, SIXEL_CMS_ENGINE_BUILTIN);
}
