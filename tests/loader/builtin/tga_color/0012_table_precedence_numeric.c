/* SPDX-License-Identifier: MIT */
/* Verify TGA table precedence independently of the decoder/CMS. */
#include "src/cms.h"
#include "tests/loader/tga_color_test_common.h"

int
test_tgac_0012(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return tga_color_test(12, SIXEL_CMS_ENGINE_BUILTIN);
}
