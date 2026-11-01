/* SPDX-License-Identifier: MIT */
/*
 * Policy: docs/encoder/output/gamut-mapping.md
 */
#include <stdio.h>
#include <float.h>
#include "src/gamut.h"
/* Isolate all private names for normal and amalgamated test runners.
 * This seam tests degenerate geometry without adding a library API. */
#define sixel_edge_build ray_test_edge_build
#define sixel_edge_compare ray_test_edge_compare
#define sixel_edge_entry ray_test_edge_entry
#define sixel_edge_entry_t ray_test_edge_entry_t
#define sixel_edge_lerp ray_test_edge_lerp
#define sixel_edge_lookup ray_test_edge_lookup
#define sixel_edge_samples ray_test_edge_samples
#define sixel_edge_scratch ray_test_edge_scratch
#define sixel_edge_scratch_t ray_test_edge_scratch_t
#define sixel_edge_step ray_test_edge_step
#define sixel_edge_unfold ray_test_edge_unfold
#define sixel_gamut ray_test_gamut
#define sixel_gamut_apply ray_test_gamut_apply
#define sixel_gamut_apply_float32 ray_test_gamut_apply_float32
#define sixel_gamut_clip ray_test_gamut_clip
#define sixel_gamut_clone ray_test_gamut_clone
#define sixel_gamut_contains ray_test_gamut_contains
#define sixel_gamut_destroy ray_test_gamut_destroy
#define sixel_gamut_edge_seeker ray_test_gamut_edge_seeker
#define sixel_gamut_edge_seeker_vtbl ray_test_gamut_edge_seeker_vtbl
#define sixel_gamut_free ray_test_gamut_free
#define sixel_gamut_local_minde ray_test_gamut_local_minde
#define sixel_gamut_local_minde_vtbl ray_test_gamut_local_minde_vtbl
#define sixel_gamut_matrix ray_test_gamut_matrix
#define sixel_gamut_new ray_test_gamut_new
#define sixel_gamut_ray_trace ray_test_gamut_ray_trace
#define sixel_gamut_ray_trace_vtbl ray_test_gamut_ray_trace_vtbl
#define sixel_gamut_vtbl ray_test_gamut_vtbl
#define sixel_gamut_vtbl_t ray_test_gamut_vtbl_t
#define sixel_ray_box ray_test_ray_box
#define sixel_gamut_t ray_test_gamut_t
typedef struct ray_test_gamut ray_test_gamut_t;
#include "src/gamut.c"
#undef sixel_edge_build
#undef sixel_edge_compare
#undef sixel_edge_entry
#undef sixel_edge_entry_t
#undef sixel_edge_lerp
#undef sixel_edge_lookup
#undef sixel_edge_samples
#undef sixel_edge_scratch
#undef sixel_edge_scratch_t
#undef sixel_edge_step
#undef sixel_edge_unfold
#undef sixel_gamut
#undef sixel_gamut_apply
#undef sixel_gamut_apply_float32
#undef sixel_gamut_clip
#undef sixel_gamut_clone
#undef sixel_gamut_contains
#undef sixel_gamut_destroy
#undef sixel_gamut_edge_seeker
#undef sixel_gamut_edge_seeker_vtbl
#undef sixel_gamut_free
#undef sixel_gamut_local_minde
#undef sixel_gamut_local_minde_vtbl
#undef sixel_gamut_matrix
#undef sixel_gamut_new
#undef sixel_gamut_ray_trace
#undef sixel_gamut_ray_trace_vtbl
#undef sixel_gamut_vtbl
#undef sixel_gamut_vtbl_t
#undef sixel_ray_box
#undef sixel_gamut_t

int
test_gamut_0056_ray_geometry(int argc, char **argv)
{
    ray_test_gamut_t mapper = {0};
    double const starts[][3] = {
        {.5, .5, .5}, {2, .5, .5}, {2, .5, .5}, {2, .5, .5},
        {.5, .5, .5}, {.5, .5, .5}, {0, .5, .5}
    };
    double const ends[][3] = {
        {2, .5, .5}, {0, .5, .5}, {3, .5, .5}, {2, 2, .5},
        {.5, .5, .5}, {.75, .5, .5}, {1, .5, .5}
    };
    int const hits[] = {1, 1, 0, 0, 0, 1, 1};
    double result[3];
    double input[3] = {-.5, .3, .1};
    double expected[3] = {.2, .3, .2};
    double huge[3] = {4.0, 0.0, 0.0};
    int ok;
    int i;
    int c;

    (void)argc;
    (void)argv;
    for (c = 0; c < 3; ++c) {
        mapper.maximum[c] = 1.0;
        mapper.forward[c * 4] = mapper.backward[c * 4] = 1.0;
    }
    ok = 1;
    for (i = 0; ok && i < 7; ++i) {
        ok = ray_test_ray_box(&mapper, starts[i], ends[i], result) == hits[i];
        if (ok && hits[i]) {
            ok = result[0] == (i == 6 ? 0.0 : 1.0) &&
                result[1] == .5 && result[2] == .5;
        }
    }
    if (!ok) {
        fprintf(stderr, "ray box case %d failed\n", i - 1);
    }
    /* A non-unit decoded box may exclude the neutral anchor. A ray with
     * no intersection retains its last candidate, then clips to the box.
     * Check that neither NaN nor an alternate mapping policy leaks out. */
    for (c = 0; c < 3; ++c) {
        mapper.minimum[c] = .2;
        mapper.maximum[c] = .8;
    }
    ok = ok && ray_test_gamut_ray_trace(&mapper, input, result,
        SIXEL_COLORSPACE_LINEAR);
    for (c = 0; ok && c < 3; ++c) {
        ok = isfinite(result[c]) && result[c] == expected[c];
        if (!ok) {
            fprintf(stderr, "ray recovery[%d] %.17g != %.17g\n",
                c, result[c], expected[c]);
        }
    }
    /* Finite input times a finite matrix can overflow. It must fail
     * atomically rather than being treated as a ray without a hit. */
    mapper.forward[0] = DBL_MAX;
    memcpy(result, expected, sizeof(result));
    ok = ok && !ray_test_gamut_ray_trace(&mapper, huge, result,
        SIXEL_COLORSPACE_LINEAR) &&
        memcmp(result, expected, sizeof(result)) == 0;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
