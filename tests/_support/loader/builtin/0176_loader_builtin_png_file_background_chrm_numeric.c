/* SPDX-License-Identifier: MIT */
/* Transform distinct indexed foreground and bKGD before linear blending. */
#include "loader_builtin_png_metadata_test_common.h"

int
test_loader_0176_loader_builtin_png_bkgd_chrm_numeric(int argc,
    char **argv)
{
    (void)argc;
    (void)argv;
    return png_metadata_check(36, SIXEL_CMS_ENGINE_BUILTIN, 0);
}
