/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"
#include "tests/_support/phased/expected.h"

static int
phased_cancel_reuse_callback(void *context)
{
    int *calls;

    calls = context;
    return ++*calls >= 2;
}

int
test_phased_cancel_reuse(int argc, char **argv)
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
    f.run.cancel = phased_cancel_reuse_callback;
    f.run.cancel_context = &calls;
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_INTERRUPTED);
    f.run.cancel = NULL;
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_OK);
    PHASED_CHECK(memcmp(f.image->indexes, phased_expected_standard,
        PHASED_PIXELS) == 0);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
