/* SPDX-License-Identifier: MIT */
/* Sweep JPEG source/fallback allocations, checking cleanup and retry. */
#include "src/cms.h"
#include "tests/_support/loader/builtin/loader_builtin_memory_test_common.h"

int
test_bjicc_0053(int argc, char **argv)
{
    edge_loader_options_t options;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    /* ICC collection is optional, so its OOM may yield unmanaged RGB.
     * Required decoder/source allocations must still free every buffer. */
    return edge_expect_fixture_allocation_failures("CMYK16 allocations",
        "/tests/data/color-numeric/builtin-jpeg-adobe16.jpg", &options, 1);
}
