/* SPDX-License-Identifier: MIT */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../_support/processing/gpu-palette/ordered_state_common.h"

/* Evaluate just the requested voxel by scanning palette sites. This oracle
 * has no EDT, grid allocation, GPU code or floating-point intersections.
 * Axis passes break distance ties by Z, then Y, then X; a seed collision
 * keeps the last palette index. */
static int
fh_site(unsigned char const *palette, int count, int shift, int const *point)
{
    int i, c, distance, minimum, key, best_key, best, d;

    minimum = INT_MAX;
    best_key = INT_MAX;
    best = 0;
    for (i = 0; i < count; ++i) {
        distance = 0;
        key = 0;
        for (c = 0; c < 3; ++c) {
            d = (palette[3 * i + c] >> shift) - point[c];
            distance += d * d;
            key |= (palette[3 * i + c] >> shift) << (8 * c);
        }
        if (distance < minimum ||
                (distance == minimum && key <= best_key)) {
            minimum = distance;
            best_key = key;
            best = i;
        }
    }
    return best;
}

static int
fh_expected(sixel_gpu_palette_request_t const *request,
    unsigned char const *pixel)
{
    int point[3], neighbor[3], corner[3];
    int shift, best, boundary, zero, i, c, delta, candidate, distance, minimum;

    shift = request->fhedt_resolution == 64 ? 2
        : request->fhedt_resolution == 128 ? 1 : 0;
    for (c = 0; c < 3; ++c) point[c] = pixel[c] >> shift;
    best = fh_site(request->palette, request->ncolors, shift, point);
    if (!request->fhedt_refine) return best;
    boundary = 0;
    zero = 1;
    for (c = 0; c < 3; ++c) {
        if ((request->palette[3 * best + c] >> shift) != point[c]) zero = 0;
        for (delta = -1; delta <= 1; delta += 2) {
            memcpy(neighbor, point, sizeof(point));
            neighbor[c] += delta;
            if (neighbor[c] >= 0 &&
                    neighbor[c] < request->fhedt_resolution &&
                    fh_site(request->palette, request->ncolors, shift,
                        neighbor) != best) boundary = 1;
        }
    }
    if (!boundary || (request->fhedt_use_dist2 && zero)) return best;
    minimum = INT_MAX;
    for (i = 0; i < 8; ++i) {
        for (c = 0; c < 3; ++c) {
            corner[c] = point[c] + ((i >> c) & 1);
            if (corner[c] == request->fhedt_resolution) --corner[c];
        }
        candidate = fh_site(request->palette, request->ncolors, shift, corner);
        distance = 0;
        for (c = 0; c < 3; ++c) {
            delta = pixel[c] -
                ((request->palette[3 * candidate + c] >> shift) << shift);
            distance += delta * delta;
        }
        if (distance < minimum) {
            minimum = distance;
            best = candidate;
        }
    }
    return best;
}

static void
fh_request(ordered_state_fixture_t *fixture)
{
    sixel_gpu_palette_request_t *request;

    ordered_state_init(fixture, 33, 1);
    request = &fixture->request;
    request->lut_policy = SIXEL_LUT_POLICY_FHEDT;
    request->fhedt_configured = 1;
    request->fhedt_resolution = 64;
    request->fhedt_refine = 1;
    request->fhedt_weights[0] = 1;
    request->fhedt_weights[1] = 1;
    request->fhedt_weights[2] = 1;
    /* Duplicate a voxel at both ends of the palette. */
    memcpy(fixture->palette + 96, fixture->palette, 3);
}

static int
fh_check(ordered_state_fixture_t *fixture)
{
    int i, expected;
    SIXELSTATUS status;

    status = sixel_gpu_palette_apply(&fixture->request);
    if (status != SIXEL_OK) {
        fprintf(stderr, "FHEDT GPU status=%x: %s\n", status,
            sixel_helper_get_additional_message());
        return 0;
    }
    for (i = 0; i < ORDERED_STATE_PIXELS; ++i) {
        expected = fh_expected(&fixture->request, fixture->pixels + 3 * i);
        if (fixture->output[i] != expected) {
            fprintf(stderr, "FHEDT pixel=%d actual=%d expected=%d\n", i,
                (int)fixture->output[i], expected);
            return 0;
        }
    }
    return 1;
}

int
test_fhedt_gpu_reference(int argc, char **argv)
{
    ordered_state_fixture_t fixture;
    sixel_gpu_palette_test_hooks_t hooks;

    if (argc != 4) return 1;
    fh_request(&fixture);
    memset(&hooks, 0, sizeof(hooks));
    fixture.request.fhedt_resolution = atoi(argv[1]);
    fixture.request.fhedt_refine = atoi(argv[2]);
    fixture.request.fhedt_use_dist2 = atoi(argv[3]);
    fixture.request.test_hooks = &hooks;
    return fh_check(&fixture) && hooks.kernel == 7 && hooks.published ? 0 : 1;
}

/* One state transition per invocation, including a retry after one injected
 * command failure. Comparing values alone would miss unnecessary rebuilds. */
int
test_fhedt_gpu_state(int argc, char **argv)
{
    ordered_state_fixture_t fixture, other;
    sixel_gpu_palette_test_hooks_t hooks;
    int change, rebuild;
    SIXELSTATUS status;

    if (argc != 2) return 1;
    change = atoi(argv[1]);
    fh_request(&fixture);
    memset(&hooks, 0, sizeof(hooks));
    fixture.request.test_hooks = &hooks;
    if (!fh_check(&fixture) || !hooks.rebuild) return 1;
    rebuild = change != 0 && change != 6;
    if (change == 1) fixture.palette[0] ^= 127;
    if (change == 2) fixture.request.ncolors = 2;
    if (change == 3) fixture.request.fhedt_resolution = 128;
    if (change == 4) fixture.request.fhedt_refine = 0;
    if (change == 5) fixture.request.fhedt_use_dist2 = 1;
    if (change == 6 || change == 7) {
        fh_request(&other);
        other.palette[0] ^= 255;
        if (change == 6) {
            other.request.lut_policy = SIXEL_LUT_POLICY_ORDERED;
            if (!ordered_state_check(&other)) return 1;
        } else if (!fh_check(&other)) return 1;
    }
    if (change >= 8) {
        fixture.palette[0] ^= 63;
        hooks.fault = change - 7;
        status = sixel_gpu_palette_apply(&fixture.request);
        if (status != (hooks.fault == 1 ? SIXEL_BAD_ALLOCATION
                : SIXEL_RUNTIME_ERROR) || hooks.published) return 1;
        hooks.fault = 0;
    }
    return fh_check(&fixture) && hooks.rebuild == rebuild && hooks.published
        ? 0 : 1;
}
