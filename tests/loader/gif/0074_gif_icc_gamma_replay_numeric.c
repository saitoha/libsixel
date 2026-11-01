/* SPDX-License-Identifier: MIT */
/* Verify GIF ICC gamma replay. */
#include "src/cms.h"
#include "tests/_support/loader/gif_icc_test_common.h"

int
test_gifc_0037(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return gif_icc_test(37, SIXEL_CMS_ENGINE_BUILTIN);
}
