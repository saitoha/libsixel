/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"
#include "tests/_support/phased/expected.h"

static int
phased_cancel_before_callback(void *context)
{
    int *calls;

    calls = context;
    return ++*calls >= 1;
}

int
test_phased_cancel_before(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;
    int calls = 0;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    f.run.cancel = phased_cancel_before_callback;
    f.run.cancel_context = &calls;
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_INTERRUPTED);
    PHASED_CHECK(f.image->rows == 0 &&
        sixel_indexed_frame_get_state(f.image) == SIXEL_INDEXED_FAILED);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
