/* SPDX-License-Identifier: MIT */
/* Check singular cHRM against independent gamma-only values and metadata. */
#include "loader_builtin_png_metadata_test_common.h"

int
test_loader_0175_loader_builtin_png_singular_chrm_numeric(int argc,
    char **argv)
{
    (void)argc;
    (void)argv;
    return png_metadata_check(1, SIXEL_CMS_ENGINE_BUILTIN, 0);
}
