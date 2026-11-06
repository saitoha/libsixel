/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"
#include "src/lookup-policy-gpu-legacy.h"

int
test_phased_byte_lookup_rejected(int argc, char **argv)
{
    phased_fixture_t f;
    sixel_lookup_policy_configuration_t configuration;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    f.lookup->vtbl->unref(f.lookup);
    f.lookup = NULL;
    memset(&configuration, 0, sizeof(configuration));
    configuration.kind = SIXEL_LUT_POLICY_EYTZINGER;
    configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    PHASED_CHECK(sixel_gpu_lookup_legacy_create(f.allocator,
        &configuration, &f.lookup) == SIXEL_OK);
    f.prepare.gpu_lookup = f.lookup;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_FEATURE_ERROR);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
