/* SPDX-License-Identifier: MIT */
/* Check nonneutral and white samples against the adapted PNG definition. */
#include "loader_builtin_png_metadata_test_common.h"

int
test_loader_0174_loader_builtin_png_chrm_gama_float_numeric(int argc,
    char **argv)
{
    (void)argc;
    (void)argv;
    return png_metadata_check(8, SIXEL_CMS_ENGINE_BUILTIN, 0);
}
