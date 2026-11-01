/* Verify loader-owned allocation failures around builtin ICC conversion. */

#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"

int
test_loader_0169_loader_builtin_png_icc_allocation_failures(int argc,
                                                             char **argv)
{
    edge_loader_options_t options;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    return edge_expect_fixture_allocation_failures(
        "PNG builtin ICC",
        "/tests/data/inputs/formats/map8_embedded_icc.png",
        &options,
        1);
}
