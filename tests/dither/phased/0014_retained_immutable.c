/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_phased_retained_immutable(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;
    sixel_retained_read_view_t view;
    size_t i;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    PHASED_CHECK(phased_fixture_baseline(&f, 0) == SIXEL_OK);
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_OK);
    PHASED_CHECK(sixel_retained_read_get_view(f.read, &view) == SIXEL_OK);
    for (i = 0; i < PHASED_PIXELS*3; ++i) PHASED_CHECK(view.rgb[i] == 31);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
