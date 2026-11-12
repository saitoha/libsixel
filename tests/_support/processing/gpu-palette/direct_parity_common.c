/* SPDX-License-Identifier: MIT */
/* Exact CPU parity for the direct GPU palette apply (lookup NONE, the
 * exact policies, or byte FHEDT where the backend serves it). Every GPU backend that claims this
 * stage, including a partial one, must emit the same SIXEL bytes as the CPU
 * for the same palette, method, lookup settings and image. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/output.h"

#define DIRECT_PARITY_WIDTH 64
#define DIRECT_PARITY_HEIGHT 48

/* Lookup settings; resolution zero selects lookup NONE, or the exact
 * policy kind when exact is nonzero (AUTO is zero). cpu_none compares
 * with a CPU NONE encode, for a policy whose GPU contract is exact while
 * its CPU byte path is not. */
typedef struct direct_parity_lookup {
    int exact;
    int kind;
    int cpu_none;
    int fhedt_resolution;
    int fhedt_refine;
    int fhedt_use_dist2;
} direct_parity_lookup_t;

typedef struct direct_parity_stream {
    unsigned char *bytes;
    size_t size;
    size_t capacity;
} direct_parity_stream_t;

static int
direct_parity_write(char *bytes, int size, void *context)
{
    direct_parity_stream_t *stream;
    unsigned char *grown;
    size_t capacity;

    stream = context;
    if (size < 0) return -1;
    if ((size_t)size > stream->capacity - stream->size) {
        capacity = stream->capacity * 2U + (size_t)size;
        grown = realloc(stream->bytes, capacity);
        if (grown == NULL) return -1;
        stream->bytes = grown;
        stream->capacity = capacity;
    }
    memcpy(stream->bytes + stream->size, bytes, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

static unsigned int
direct_parity_next(unsigned int *state)
{
    *state = *state * 1103515245U + 12345U;
    return (*state >> 16) & 0x7fffU;
}

static int
direct_parity_encode(unsigned char *pixels, unsigned char const *palette,
    int ncolors, int method, int policy, direct_parity_lookup_t const *lookup,
    direct_parity_stream_t *stream)
{
    sixel_dither_t *dither;
    sixel_output_t *output;
    SIXELSTATUS status;

    dither = NULL;
    output = NULL;
    status = sixel_dither_new(&dither, ncolors, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_output_new(&output, direct_parity_write, stream,
            NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        sixel_dither_set_palette(dither, (unsigned char *)palette);
        dither->force_palette = 1;
        dither->bluenoise_strength_override = 1;
        dither->bluenoise_strength = 0.5f;
        dither->a_dither_strength_override = 1;
        dither->x_dither_strength_override = 1;
        dither->a_dither_strength = 0.5f;
        dither->x_dither_strength = 0.5f;
        if (lookup->exact) {
            sixel_dither_set_lut_policy(dither,
                policy == SIXEL_GPU_POLICY_OFF && lookup->cpu_none
                    ? SIXEL_LUT_POLICY_NONE : lookup->kind);
        } else if (lookup->fhedt_resolution == 0) {
            sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
        } else {
            sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_FHEDT);
            dither->lut_policy_fhedt_resolution_override = 1;
            dither->lut_policy_fhedt_resolution = lookup->fhedt_resolution;
            dither->lut_policy_fhedt_refine_override = 1;
            dither->lut_policy_fhedt_refine = lookup->fhedt_refine;
            dither->lut_policy_fhedt_use_dist2_override = 1;
            dither->lut_policy_fhedt_use_dist2 = lookup->fhedt_use_dist2;
        }
        sixel_dither_set_diffusion_type(dither, method);
        /* FORCE reports errors instead of silently falling back to CPU. */
        dither->gpu_policy = policy;
        status = sixel_encode(pixels, DIRECT_PARITY_WIDTH,
            DIRECT_PARITY_HEIGHT, 3, dither, output);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "direct parity encode (policy %d): %s\n", policy,
            sixel_helper_get_additional_message());
    }
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return SIXEL_SUCCEEDED(status) ? 0 : 1;
}

static int
direct_parity_run(int method, int ncolors, unsigned int seed,
    direct_parity_lookup_t const *lookup)
{
    unsigned char pixels[DIRECT_PARITY_WIDTH * DIRECT_PARITY_HEIGHT * 3];
    unsigned char palette[SIXEL_PALETTE_MAX * 3];
    direct_parity_stream_t streams[2];
    unsigned int state;
    int x, y, i, pass, ok;
    unsigned char *pixel;

    /* Smooth gradients exercise positional noise; the LCG term adds ties
     * and near-ties so index order in the scan is observable. */
    state = seed + (unsigned int)method * 97U + (unsigned int)ncolors;
    for (y = 0; y < DIRECT_PARITY_HEIGHT; ++y) {
        for (x = 0; x < DIRECT_PARITY_WIDTH; ++x) {
            pixel = pixels + ((size_t)y * DIRECT_PARITY_WIDTH + x) * 3U;
            pixel[0] = (unsigned char)(x * 4 + direct_parity_next(&state) % 5);
            pixel[1] = (unsigned char)(y * 5 + direct_parity_next(&state) % 5);
            pixel[2] = (unsigned char)((x + y) * 2 +
                direct_parity_next(&state) % 5);
        }
    }
    if (lookup->fhedt_resolution == 0) {
        for (i = 0; i < ncolors * 3; ++i) {
            palette[i] = (unsigned char)(direct_parity_next(&state) & 0xffU);
        }
    } else {
        /* Like a quantizer's output, colors sampled from the image cluster
         * and share table cells, which exercises boundary refinement far
         * more than uniformly random colors do. */
        for (i = 0; i < ncolors; ++i) {
            pixel = pixels + (size_t)(direct_parity_next(&state) %
                (DIRECT_PARITY_WIDTH * DIRECT_PARITY_HEIGHT)) * 3U;
            memcpy(palette + i * 3, pixel, 3);
        }
    }
    memset(streams, 0, sizeof(streams));
    ok = 0;
    for (pass = 0; pass < 2; ++pass) {
        if (direct_parity_encode(pixels, palette, ncolors, method,
                pass ? SIXEL_GPU_POLICY_FORCE : SIXEL_GPU_POLICY_OFF,
                lookup, &streams[pass])) goto end;
    }
    if (streams[0].size == 0 || streams[0].size != streams[1].size ||
            memcmp(streams[0].bytes, streams[1].bytes, streams[0].size)) {
        fprintf(stderr, "direct parity mismatch method=%d colors=%d "
            "lut=%d fhedt=R%d/refine%d/dist2%d seed=%#x cpu=%zu gpu=%zu "
            "bytes\n", method, ncolors, lookup->exact ? lookup->kind : -1,
            lookup->fhedt_resolution, lookup->fhedt_refine,
            lookup->fhedt_use_dist2, seed, streams[0].size,
            streams[1].size);
        goto end;
    }
    ok = 1;
end:
    free(streams[0].bytes);
    free(streams[1].bytes);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_gpu_direct_parity(int method, int ncolors)
{
    direct_parity_lookup_t lookup;

    memset(&lookup, 0, sizeof(lookup));
    return direct_parity_run(method, ncolors, 0x5eed1234U, &lookup);
}

/* Byte FHEDT at one resolution. A repeated seed repeats the palette, so a
 * backend that caches its table reuses it; a distinct seed forces a rebuild,
 * and returning to the first seed rebuilds over a different table. */
int
test_gpu_direct_parity_fhedt(int method, int ncolors, int resolution,
    int refine, int use_dist2)
{
    static unsigned int const seeds[] = {
        0x5eed1234U, 0x5eed1234U, 0x0badf00dU, 0x5eed1234U
    };
    direct_parity_lookup_t lookup;
    size_t i;

    lookup.fhedt_resolution = resolution;
    lookup.fhedt_refine = refine;
    lookup.fhedt_use_dist2 = use_dist2;
    for (i = 0; i < sizeof(seeds) / sizeof(seeds[0]); ++i) {
        if (direct_parity_run(method, ncolors, seeds[i], &lookup) !=
                EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/* Every exact policy, AUTO included, returns the nearest entry with the
 * lowest index on ties, so each must match the CPU for every per-pixel
 * method. The CPU certlut byte cache is approximate by design; its exact
 * GPU contract is checked against CPU NONE instead. */
int
test_gpu_direct_parity_exact(int ncolors)
{
    static int const kinds[] = {
        SIXEL_LUT_POLICY_AUTO, SIXEL_LUT_POLICY_VPTREE,
        SIXEL_LUT_POLICY_DELAUNAY, SIXEL_LUT_POLICY_BLOCKCULL,
        SIXEL_LUT_POLICY_ORDERED, SIXEL_LUT_POLICY_GRIDLUT,
        SIXEL_LUT_POLICY_MAHALANOBIS, SIXEL_LUT_POLICY_RBC,
        SIXEL_LUT_POLICY_CERTLUT
    };
    static int const methods[] = {
        SIXEL_DIFFUSE_NONE, SIXEL_DIFFUSE_A_DITHER, SIXEL_DIFFUSE_X_DITHER,
        SIXEL_DIFFUSE_BLUENOISE_DITHER
    };
    direct_parity_lookup_t lookup;
    size_t i, j;

    memset(&lookup, 0, sizeof(lookup));
    lookup.exact = 1;
    for (i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        for (j = 0; j < sizeof(methods) / sizeof(methods[0]); ++j) {
            lookup.kind = kinds[i];
            lookup.cpu_none = kinds[i] == SIXEL_LUT_POLICY_CERTLUT;
            if (direct_parity_run(methods[j], ncolors, 0x5eed1234U, &lookup)
                    != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}
