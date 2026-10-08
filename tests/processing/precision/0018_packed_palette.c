/* SPDX-License-Identifier: MIT */
/* Packed source stride does not determine palette coordinate precision. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/palette.h"

#define PACKED_PIXELS 7

static int packed_track;
static size_t packed_fail_size;
static int packed_fail_hits;
static void *packed_buffers[2];
static int packed_allocs[2];
static int packed_frees[2];
static void *packed_borrowed;
static int packed_borrowed_frees;

static void *
packed_malloc(size_t size)
{
    void *result;
    int slot;

    if (packed_track && packed_fail_size == size) {
        packed_fail_size = 0;
        ++packed_fail_hits;
        return NULL;
    }
    result = malloc(size);
    if (packed_track) {
        slot = size == PACKED_PIXELS * 3 ? 0 :
            size == PACKED_PIXELS * 3 * sizeof(float) ? 1 : -1;
        if (slot >= 0 && ++packed_allocs[slot] == 1) {
            /* Follow the first normalization/promotion allocation only.
             * A diffusion policy may allocate another buffer of this size. */
            packed_buffers[slot] = result;
        }
    }
    return result;
}

static void
packed_free(void *pointer)
{
    int slot;

    if (pointer != NULL && pointer == packed_borrowed) {
        ++packed_borrowed_frees;
        return;
    }
    for (slot = 0; slot < 2; ++slot) {
        if (pointer != NULL && pointer == packed_buffers[slot]) {
            ++packed_frees[slot];
            packed_buffers[slot] = NULL;
        }
    }
    free(pointer);
}

static int
packed_case(int scenario, int format, int alpha_offset)
{
    /* The byte palette has midpoint 77 and chooses slot 0 at a tie.
     * The stored float midpoint is .3005, so 77/255 must choose slot 1.
     * Grays isolate precision from the independently tested RGB swizzle. */
    unsigned char palette_bytes[9] = {51, 51, 51, 103, 103, 103, 255, 255, 255};
    float palette_values[9] = {
        .199f, .199f, .199f, .402f, .402f, .402f, 1.f, 1.f, 1.f
    };
    unsigned char const levels[PACKED_PIXELS] = {77, 51, 103, 77, 51, 103, 77};
    sixel_index_t const expected[PACKED_PIXELS] = {1, 0, 1, 1, 0, 1, 1};
    sixel_index_t const byte_expected[PACKED_PIXELS] = {0, 0, 1, 0, 0, 1, 0};
    sixel_index_t const mask_expected[PACKED_PIXELS] = {1, 2, 1, 1, 0, 1, 1};
    unsigned char background[3] = {0, 0, 0};
    unsigned char pixels[PACKED_PIXELS * 4];
    unsigned char saved[sizeof(pixels)];
    sixel_index_t borrowed[PACKED_PIXELS];
    sixel_index_t untouched[PACKED_PIXELS];
    sixel_palette_float32_entries_request_t request;
    sixel_palette_float32_entries_view_t view;
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_index_t *indices;
    sixel_index_t const *want;
    int alpha;
    int masked;
    int float_palette;
    int ok;
    int index;
    int channel;
    int attempt;
    int failure;

    allocator = NULL;
    dither = NULL;
    indices = NULL;
    ok = 0;
    packed_track = 0;
    packed_fail_size = 0;
    packed_fail_hits = 0;
    packed_borrowed = NULL;
    packed_borrowed_frees = 0;
    masked = scenario == 3 || scenario == 4 || scenario == 8;
    float_palette = scenario != 5;
    want = masked ? mask_expected : float_palette ? expected : byte_expected;
    for (index = 0; index < PACKED_PIXELS; ++index) {
        alpha = masked && index == 1 ? 0 :
            masked && index == 3 ? 128 : 255;
        for (channel = 0; channel < 4; ++channel) {
            pixels[index * 4 + channel] = channel == alpha_offset ?
                (unsigned char)alpha : scenario == 4 && index == 3 ?
                154 : levels[index];
        }
        borrowed[index] = (sixel_index_t)19;
    }
    memcpy(saved, pixels, sizeof(saved));
    memcpy(untouched, borrowed, sizeof(untouched));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, packed_malloc,
            NULL, NULL, packed_free)) ||
            SIXEL_FAILED(sixel_dither_new(&dither, 3, allocator))) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette_bytes);
    sixel_dither_set_lut_policy(dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_diffusion_type(dither,
        scenario == 2 ? SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_NONE);
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
            SIXEL_DITHER_CLAMP_HARD, 1.01))) {
        goto end;
    }
    dither->force_palette = 1;
    if (float_palette) {
        memset(&request, 0, sizeof(request));
        request.entries = palette_values;
        request.colors = 3;
        request.depth = 3 * sizeof(float);
        if (SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
                dither->palette, &request))) {
            goto end;
        }
    }
    if (masked) {
        sixel_dither_set_transparent(dither, 2);
    }
    if (scenario == 4) {
        /* Existing gamma byte composition rounds 154 at alpha 128 over
         * black to 77. Palette application must retain its float midpoint. */
        memcpy(dither->transparent_bgcolor, background, sizeof(background));
        dither->transparent_bgcolor_valid = 1;
    }
    for (attempt = 0; attempt < (scenario == 8 ? 2 : 1); ++attempt) {
        failure = scenario >= 6 && attempt == 0;
        memset(packed_buffers, 0, sizeof(packed_buffers));
        memset(packed_allocs, 0, sizeof(packed_allocs));
        memset(packed_frees, 0, sizeof(packed_frees));
        packed_fail_size = failure ? PACKED_PIXELS * 3 *
            (scenario == 6 ? 1 : sizeof(float)) :
            scenario == 5 ? PACKED_PIXELS * 3 * sizeof(float) : 0;
        /* Set the next source's format on each call, including retries.
         * Normalization currently records RGB888 in the dither object. */
        sixel_dither_set_pixelformat(dither, format);
        if (scenario == 8) {
            dither->pipeline_index_buffer = borrowed;
            dither->pipeline_index_size = sizeof(borrowed);
            packed_borrowed = borrowed;
        }
        packed_track = 1;
        indices = sixel_dither_apply_palette(dither, pixels, PACKED_PIXELS, 1);
        packed_track = 0;
        memset(&view, 0, sizeof(view));
        ok = memcmp(pixels, saved, sizeof(saved)) == 0 &&
            SIXEL_SUCCEEDED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) &&
            (float_palette ? view.entries != NULL && view.entry_count == 3 &&
             view.depth == 3 * sizeof(float) &&
             memcmp(view.entries, palette_values, sizeof(palette_values)) == 0
             : view.entries == NULL) &&
            dither->use_rgb101_palette == 0 &&
            packed_borrowed_frees == 0 &&
            dither->pipeline_transparent_mask == NULL;
        if (failure) {
            ok = ok && indices == NULL && packed_fail_hits == 1 &&
                dither->interframe_state.last_apply_status ==
                    SIXEL_BAD_ALLOCATION &&
                dither->interframe_state.consume_count == 0 &&
                packed_allocs[1] == 0 && packed_frees[1] == 0 &&
                packed_frees[0] == (scenario == 6 ? 0 : 1) &&
                (scenario != 8 ||
                 memcmp(borrowed, untouched, sizeof(borrowed)) == 0);
        } else {
            ok = ok && indices != NULL &&
                dither->interframe_state.last_apply_status == SIXEL_OK &&
                dither->interframe_state.depth ==
                    (float_palette ? 3 * (int)sizeof(float) : 3) &&
                dither->interframe_state.consume_count == 1 &&
                packed_frees[0] == 1 &&
                packed_frees[1] == (float_palette ? 1 : 0) &&
                (scenario != 5 || packed_fail_hits == 0) &&
                (scenario != 8 || indices == borrowed) &&
                (scenario == 2 ? indices[0] == 1 :
                 memcmp(indices, want, sizeof(expected)) == 0);
        }
        if (indices != NULL && indices != borrowed) {
            sixel_allocator_free(allocator, indices);
        }
        indices = NULL;
        if (!ok) {
            goto end;
        }
    }
end:
    packed_track = 0;
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    ok = ok && packed_borrowed_frees == 0;
    if (!ok) {
        fprintf(stderr, "packed case=%d format=%d hits=%d frees=%d/%d\n",
            scenario, format, packed_fail_hits,
            packed_frees[0], packed_frees[1]);
    }
    return ok;
}

int
test_precision_0018(int argc, char **argv)
{
    int const alpha_formats[] = {SIXEL_PIXELFORMAT_RGBA8888,
        SIXEL_PIXELFORMAT_BGRA8888, SIXEL_PIXELFORMAT_ARGB8888,
        SIXEL_PIXELFORMAT_ABGR8888};
    int const padding_formats[] = {SIXEL_PIXELFORMAT_RGBX8888,
        SIXEL_PIXELFORMAT_BGRX8888, SIXEL_PIXELFORMAT_XRGB8888,
        SIXEL_PIXELFORMAT_XBGR8888};
    int scenario;
    int index;
    int format;

    scenario = argc == 2 ? atoi(argv[1]) : -1;
    if (scenario < 0 || scenario > 8) {
        return 1;
    }
    for (index = 0; index < (scenario < 2 ? 4 : 1); ++index) {
        format = scenario == 1 ? padding_formats[index] : alpha_formats[index];
        if (!packed_case(scenario, format, index < 2 ? 3 : 0)) {
            return 1;
        }
    }
    return 0;
}
