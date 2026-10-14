/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "src/progressive-palette.h"

static int
repair_test_checkpoint(void)
{
    sixel_repair_sample_t samples[512];
    float candidates[2][3] = { { 0, 0, 0 }, { 1, 1, 1 } };
    sixel_repair_palette_t solver;
    sixel_repair_work_t work;
    size_t i;
    unsigned int calls;
    int done;

    memset(samples, 0, sizeof(samples));
    for (i = 0; i < 512; ++i) {
        memcpy(samples[i].source, candidates[i & 1],
               sizeof(samples[i].source));
        samples[i].error = 3;
        samples[i].population = 1;
    }
    if (sixel_repair_palette_begin(&solver, samples, 512,
            (float const (*)[3])candidates, 2, 2, 0, 0, 2) != SIXEL_OK) {
        return 0;
    }
    done = 0;
    for (calls = 0; calls < 1000 && !done; ++calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = 100000;
        if (sixel_repair_palette_advance(&solver, &work, &done) != SIXEL_OK ||
                work.pixel_visits > 256 || work.distance_evaluations > 256) {
            return 0;
        }
    }
    /* Even a generous caller budget must return control between chunks so
     * the owner can observe cancellation and a waiting ordinary frame. */
    return done && calls > 4 && solver.colors == 2 && solver.cost == 0;
}

static int
repair_test_run(sixel_repair_palette_t *solver, size_t quantum,
                size_t *evaluations)
{
    sixel_repair_work_t work;
    unsigned int calls;
    int done;

    done = 0;
    *evaluations = 0;
    for (calls = 0; calls < 10000 && !done; ++calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = quantum;
        work.distance_limit = quantum;
        if (sixel_repair_palette_advance(solver, &work, &done) != SIXEL_OK ||
                work.pixel_visits > quantum ||
                work.distance_evaluations > quantum) {
            return 0;
        }
        *evaluations += work.distance_evaluations;
    }
    return done;
}

int
test_repair_palette_case(int mode)
{
    float candidates[4][3];
    sixel_repair_sample_t samples[4];
    sixel_repair_sample_t repeated[4];
    sixel_repair_palette_t solver;
    sixel_repair_palette_t replay;
    size_t evaluations;
    size_t replay_evaluations;
    size_t count;
    unsigned int i;
    unsigned int cap;
    int protected_candidate;
    double cutoff;
    int ok;

    memset(candidates, 0, sizeof(candidates));
    memset(samples, 0, sizeof(samples));
    candidates[1][0] = 0.8f;
    candidates[2][0] = 0.9f;
    candidates[3][0] = 1.0f;
    count = 4;
    cap = 2;
    cutoff = mode == 4 ? 0.2 : 0.0;
    protected_candidate = mode == 6 ? 1 : -1;
    for (i = 0; i < count; ++i) {
        samples[i].source[0] = candidates[i][0];
        samples[i].population = 1;
        samples[i].error = 1;
    }
    if (mode == 3) {
        /* A normal palette-only objective chooses the dominant black
         * sample. Keeping that already-correct pixel lets repair choose
         * white for the only unresolved sample instead. */
        count = 2;
        cap = 1;
        candidates[1][0] = 1;
        samples[1].source[0] = 1;
        samples[0].population = 100;
        samples[0].error = 0;
    }
    memcpy(repeated, samples, sizeof(samples));
    if (sixel_repair_palette_begin(&solver, samples, count,
            (float const (*)[3])candidates, (unsigned int)count,
            cap, protected_candidate, cutoff, 2) != SIXEL_OK ||
            !repair_test_run(&solver, 10000, &evaluations)) {
        return EXIT_FAILURE;
    }
    ok = 0;
    if (mode == 3) {
        ok = solver.colors == 1 && solver.palette[0] == 1 &&
            solver.cost == 0 && solver.initial_cost == 1;
    } else if (mode == 4) {
        ok = solver.colors == 1 && solver.palette[0] == 1 &&
            fabs(solver.cost - 0.69) < 1e-6;
    } else if (mode == 5) {
        /* Greedy picks 0.8 then 0.0, cost 0.05. Replacing 0.8 by 0.9
         * leaves cost 0.02. These literal costs include each keep option. */
        ok = solver.colors == 2 && solver.palette[0] == 2 &&
            solver.palette[1] == 0 && solver.swaps == 1 &&
            fabs(solver.cost - 0.02) < 1e-6;
    } else if (mode == 6) {
        ok = solver.colors == 2 && solver.palette[0] == 1 &&
            solver.palette[1] == 0 && solver.swaps == 0 &&
            fabs(solver.cost - 0.05) < 1e-6;
    } else if (mode == 7) {
        ok = sixel_repair_palette_begin(&replay, repeated, count,
            (float const (*)[3])candidates, (unsigned int)count,
            cap, protected_candidate, cutoff, 2) == SIXEL_OK &&
            repair_test_run(&replay, 1, &replay_evaluations);
        ok = ok && replay.colors == solver.colors &&
            replay.palette[0] == solver.palette[0] &&
            replay.palette[1] == solver.palette[1] &&
            replay.swaps == solver.swaps && replay.cost == solver.cost &&
            evaluations == replay_evaluations;
        ok = ok && repair_test_checkpoint();
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
