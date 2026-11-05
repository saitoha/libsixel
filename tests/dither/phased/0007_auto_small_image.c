/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"
#include "tests/_support/phased/expected.h"

int
test_phased_auto_small_image(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    f.prepare.gpu_policy = SIXEL_GPU_POLICY_AUTO;
    f.prepare.gpu_auto_threshold = 1000000;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_OK);
    PHASED_CHECK(memcmp(f.image->indexes, phased_expected_standard,
        PHASED_PIXELS) == 0);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
