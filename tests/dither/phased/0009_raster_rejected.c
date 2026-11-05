/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_phased_raster_rejected(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 0);
    if (outcome != 0) goto end;
    outcome = 1;
    f.configuration.method_for_scan = SIXEL_SCAN_RASTER;
    PHASED_CHECK(f.dither->vtbl->configure(f.dither, &f.configuration)
        == SIXEL_FEATURE_ERROR);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
