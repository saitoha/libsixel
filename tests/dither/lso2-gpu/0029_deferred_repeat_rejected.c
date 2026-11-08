/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected.h"

int
test_lso2_gpu_deferred_repeat_rejected(int argc, char **argv)
{
    lso2_gpu_fixture_t f;
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
    outcome = lso2_gpu_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(f.apply->vtbl->get_deferred(f.apply, &cap) == SIXEL_OK);
    solve.source = f.run.source;
    LSO2_GPU_CHECK(cap->vtbl->solve(cap, &solve, &solution) == SIXEL_OK);
    LSO2_GPU_CHECK(sixel_indexed_frame_new(&f.image, &f.prepare.output,
        f.allocator) == SIXEL_OK);
    resolve.result = f.image;
    LSO2_GPU_CHECK(solution->vtbl->resolve(solution, &resolve) == SIXEL_OK);
    LSO2_GPU_CHECK(solution->vtbl->resolve(solution, &resolve)
        == SIXEL_LOGIC_ERROR);
    outcome = 0;
end:
    if (solution != NULL) solution->vtbl->unref(solution);
    if (cap != NULL) cap->vtbl->unref(cap);
    lso2_gpu_fixture_close(&f);
    return outcome;
}
