/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_phased_gpu_off_rejected(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 0);
    if (outcome != 0) goto end;
    outcome = 1;
    f.prepare.gpu_policy = SIXEL_GPU_POLICY_OFF;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_FEATURE_ERROR);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
