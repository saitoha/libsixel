/* Verify allocation-failure cleanup through PSD layer-effect rendering. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0168_loader_builtin_psd_effect_allocation_failures(int argc,
                                                                char **argv)
{
    edge_loader_options_t options;

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_fixture_allocation_failures(
        "PSD layer effects",
        "/tests/data/inputs/formats/"
        "snake16_cmyk16_missing_composite_multilayer_layer_effects.psd",
        &options,
        0);
}
