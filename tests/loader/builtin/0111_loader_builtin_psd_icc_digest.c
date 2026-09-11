/* Fix builtin-CMS output for an embedded-profile PSD. */

#include "src/cms.h"
#include "loader_builtin_memory_test_common.h"

int
test_loader_0111_loader_builtin_psd_icc_digest(int argc, char **argv)
{
    edge_loader_options_t options;
    uint64_t const digest[1] = { UINT64_C(0x6098a367760078f0) };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    return edge_expect_fixture_digests_options(
        "PSD embedded ICC",
        "/tests/data/inputs/formats/snake-64-embedded-esrgb.psd",
        &options,
        64,
        64,
        1,
        SIXEL_PIXELFORMAT_RGB888,
        SIXEL_COLORSPACE_GAMMA,
        digest);
}
