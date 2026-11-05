/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"
#include "tests/_support/phased/expected.h"

int
test_phased_preparer_lifetime(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;
    sixel_gpu_dither_preparer_interface_t *borrowed;
    sixel_dither_policy_prepare_request_t actual;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    borrowed = f.dither;
    f.dither->vtbl->unref(f.dither);
    f.dither = NULL;
    PHASED_CHECK(borrowed->vtbl->get_configuration(borrowed, &actual)
        == SIXEL_OK);
    PHASED_CHECK(actual.options.method_for_diffuse == SIXEL_DIFFUSE_PHASED);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
