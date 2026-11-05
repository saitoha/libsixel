/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_phased_invalid_gain_atomic(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;
    sixel_dither_policy_prepare_request_t actual;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 0);
    if (outcome != 0) goto end;
    outcome = 1;
    PHASED_CHECK(f.dither->vtbl->configure(f.dither, &f.configuration)
        == SIXEL_OK);
    f.configuration.options.phased_gain = (sixel_phased_gain_t)9;
    PHASED_CHECK(f.dither->vtbl->configure(f.dither, &f.configuration)
        == SIXEL_BAD_ARGUMENT);
    PHASED_CHECK(f.dither->vtbl->get_configuration(f.dither, &actual)
        == SIXEL_OK &&
        actual.options.phased_gain == SIXEL_PHASED_GAIN_STANDARD);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
