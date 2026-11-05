/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"

int
test_phased_byte_lookup_rejected(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;

    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    f.lookup->vtbl->unref(f.lookup);
    f.lookup = NULL;
    PHASED_CHECK(sixel_gpu_lookup_vptree_create(f.allocator, &f.lookup)
        == SIXEL_OK);
    f.prepare.gpu_lookup = f.lookup;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_FEATURE_ERROR);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
