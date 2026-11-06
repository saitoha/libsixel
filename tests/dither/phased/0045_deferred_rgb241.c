/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_phased_deferred_rgb241(int argc, char **argv)
{
    phased_fixture_t f;
    unsigned char expected[PHASED_PIXELS];
    int outcome;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    PHASED_CHECK(phased_fixture_baseline(&f, 0) == SIXEL_OK);
    f.run.delta.metric = SIXEL_DELTA_METRIC_RGB241;
    f.run.delta.threshold = 48;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_OK);
    memcpy(expected, f.image->indexes, sizeof(expected));
    sixel_indexed_frame_unref(f.image);
    f.image = NULL;
    PHASED_CHECK(phased_fixture_deferred(&f) == SIXEL_OK);
    PHASED_CHECK(memcmp(f.image->indexes, expected, sizeof(expected)) == 0);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
