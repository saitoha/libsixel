/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"
#include "tests/_support/phased/expected.h"

int
test_phased_deferred_prepare_independent(int argc, char **argv)
{
    phased_fixture_t f;
    sixel_palette_apply_deferred_interface_t *cap;
    sixel_palette_apply_solution_interface_t *solution;
    sixel_palette_apply_solve_request_t solve;
    sixel_palette_apply_resolve_request_t resolve;
    int outcome;

    (void)argc;
    (void)argv;
    cap = NULL;
    solution = NULL;
    memset(&solve, 0, sizeof(solve));
    memset(&resolve, 0, sizeof(resolve));
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK(f.apply->vtbl->get_deferred(f.apply, &cap) == SIXEL_OK);
    solve.source = f.run.source;
    PHASED_CHECK(cap->vtbl->solve(cap, &solve, &solution) == SIXEL_OK);
    PHASED_CHECK(sixel_indexed_frame_new(&f.image, &f.prepare.output,
        f.allocator) == SIXEL_OK);
    resolve.result = f.image;
    f.configuration.options.phased_gain = SIXEL_PHASED_GAIN_SMALL_ERROR;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK(solution->vtbl->resolve(solution, &resolve) == SIXEL_OK);
    PHASED_CHECK(memcmp(f.image->indexes, phased_expected_standard,
        PHASED_PIXELS) == 0);
    outcome = 0;
end:
    if (solution != NULL) solution->vtbl->unref(solution);
    if (cap != NULL) cap->vtbl->unref(cap);
    phased_fixture_close(&f);
    return outcome;
}
