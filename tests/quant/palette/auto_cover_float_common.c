/* SPDX-License-Identifier: MIT */
/* Float AUTO must bypass repair without losing authoritative palette values. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-common-cover.h"

static int auto_cover_allocations;
static int auto_cover_deny;

static void *
auto_cover_calloc(size_t count, size_t size)
{
    /* The native soft candidate histogram has 4096 cells. No palette or
     * source allocation in these tiny fixtures uses that shape. */
    if (auto_cover_deny && count == 4096u) {
        ++auto_cover_allocations;
        return NULL;
    }
    return calloc(count, size);
}

int
test_auto_cover_values(unsigned int count, unsigned int capacity,
                       int mode, int grow, int format)
{
    sixel_allocator_t *allocator;
    float entries[SIXEL_PALETTE_MAX * 3];
    float original[SIXEL_PALETTE_MAX * 3];
    float samples[6] = { -2, 3, .5f, -2, 3, .5f };
    float input[6];
    unsigned int actual;
    unsigned int i;
    SIXELSTATUS status;
    int ok;

    allocator = NULL;
    for (i = 0; i < SIXEL_PALETTE_MAX; ++i) {
        /* Exact binary fractions expose clipping and byte reconstruction.
         * Initialize unused slots too: even growth must be a complete no-op. */
        entries[i * 3] = -.25f + i / 1024.0f;
        entries[i * 3 + 1] = 1.25f + i / 1024.0f;
        entries[i * 3 + 2] = i / 512.0f;
    }
    memcpy(original, entries, sizeof(entries));
    memcpy(input, samples, sizeof(samples));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL,
            auto_cover_calloc, NULL, NULL))) {
        return EXIT_FAILURE;
    }
    auto_cover_allocations = 0;
    auto_cover_deny = 1;
    actual = count;
    status = sixel_palette_cover_float(entries, &actual, samples,
        sizeof(samples), format, SIXEL_PALETTE_COVER_AUTO,
        mode, grow, capacity, allocator);
    auto_cover_deny = 0;
    ok = SIXEL_SUCCEEDED(status) && actual == count &&
        auto_cover_allocations == 0 &&
        memcmp(entries, original, sizeof(entries)) == 0 &&
        memcmp(samples, input, sizeof(samples)) == 0;
    if (!ok) {
        fprintf(stderr, "auto count=%u actual=%u mode=%d status=%x alloc=%d\n",
            count, actual, mode, status, auto_cover_allocations);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
test_auto_cover_explicit(void)
{
    sixel_allocator_t *allocator;
    float entries[SIXEL_PALETTE_MAX * 3] = { 0 };
    float original[SIXEL_PALETTE_MAX * 3];
    float samples[6] = { -.25f, 1.25f, .5f, -.25f, 1.25f, .5f };
    unsigned int count;
    int ok;

    allocator = NULL;
    ok = 0;
    count = 32;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL,
            NULL, NULL, NULL))) {
        return EXIT_FAILURE;
    }
    memcpy(original, entries, sizeof(entries));
    if (SIXEL_FAILED(sixel_palette_cover_float(entries, &count, samples,
            sizeof(samples), SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_PALETTE_COVER_AUTO, SIXEL_PALETTE_COVER_MODE_SOFT,
            1, 256, allocator)) || count != 32 ||
            memcmp(entries, original, sizeof(entries))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_palette_cover_float(entries, &count, samples,
            sizeof(samples), SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_PALETTE_COVER_CORNERS, SIXEL_PALETTE_COVER_MODE_SOFT,
            1, 256, allocator)) || count != 33 ||
            memcmp(entries, original, 32 * 3 * sizeof(float)) ||
            memcmp(entries + 32 * 3, samples, 3 * sizeof(float))) {
        goto end;
    }
    /* AUTO after an explicit call has no lingering budget or mode state. */
    memcpy(original, entries, sizeof(entries));
    ok = SIXEL_SUCCEEDED(sixel_palette_cover_float(entries, &count, samples,
        sizeof(samples), SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_PALETTE_COVER_AUTO, SIXEL_PALETTE_COVER_MODE_HARD,
        1, 256, allocator)) && count == 33 &&
        memcmp(entries, original, sizeof(entries)) == 0;
end:
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

typedef struct auto_cover_capture {
    char bytes[8192];
    size_t length;
} auto_cover_capture_t;

static int
auto_cover_write(char *data, int size, void *context)
{
    auto_cover_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->bytes) - capture->length) {
        return -1;
    }
    memcpy(capture->bytes + capture->length, data, (size_t)size);
    capture->length += (size_t)size;
    capture->bytes[capture->length] = '\0';
    return size;
}

int
test_auto_cover_encoder(int hard)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { 'p', "256" }, { 'F', "none" },
        { '_', "none" }, { '~', "none" }, { 'd', "none" },
        { 't', "rgb" }, { '4', "full-frame" }, { '5', "none" },
        { 'Q', "kmeans:seed=1" }, { '.', "float" },
        { 'X', "gamma" }, { 'W', "gamma" }, { 'U', "gamma" }
    };
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    auto_cover_capture_t capture;
    float pixels[32 * 3];
    float original[32 * 3];
    char definition[80];
    unsigned int i;
    unsigned int j;
    unsigned int code;
    unsigned int seen;
    int ok;
    SIXELSTATUS status;
    int stage;

    stage = 0;
    memset(&view, 0, sizeof(view));
    allocator = NULL;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    ok = 0;
    memset(&capture, 0, sizeof(capture));
    for (i = 0; i < 32 * 3; ++i) {
        pixels[i] = 1.0f / 512 + (i / 3) / 64.0f;
    }
    memcpy(original, pixels, sizeof(pixels));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL,
            auto_cover_calloc, NULL, NULL)) ||
            SIXEL_FAILED(sixel_encoder_new(&encoder, allocator)) ||
            SIXEL_FAILED(sixel_output_new(&output, auto_cover_write,
                &capture, allocator))) {
        goto end;
    }
    for (i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[i].flag, options[i].value))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a',
            hard ? "auto:Whard:V1" : "auto:Wsoft:V1")) ||
            SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
            SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 32, 1,
                SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    stage = 1;
    auto_cover_allocations = 0;
    auto_cover_deny = 1;
    status = sixel_encoder_encode_frame(encoder, frame, output);
    auto_cover_deny = 0;
    dither = (sixel_dither_t *)encoder->diagnostic_dither;
    if (SIXEL_FAILED(status) || auto_cover_allocations != 0 ||
            memcmp(pixels, original, sizeof(pixels)) || dither == NULL ||
            dither->use_rgb101_palette == 0 ||
            dither->pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
            SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != 32) {
        goto end;
    }
    /* All 32 exact source colors fit the requested 256 entries. Check the
     * actual count and each authoritative triple, independent of ordering.
     * Each RGB101 definition follows from the same binary fraction; none
     * is close to a half-integer rounding boundary. */
    stage = 2;
    seen = 0;
    for (i = 0; i < view.entry_count; ++i) {
        for (j = 0; j < 32; ++j) {
            if (memcmp(view.entries + i * 3, original + j * 3,
                    3 * sizeof(float)) == 0) {
                break;
            }
        }
        if (j == 32 || (seen & (1u << j))) {
            fprintf(stderr, "entry %u = %.9g %.9g %.9g\n", i,
                (double)view.entries[i * 3],
                (double)view.entries[i * 3 + 1],
                (double)view.entries[i * 3 + 2]);
            goto end;
        }
        seen |= 1u << j;
        code = (unsigned int)(original[j * 3] * 100.0 + .5);
        snprintf(definition, sizeof(definition), "#%u;2;%u;%u;%u",
            i, code, code, code);
        if (strstr(capture.bytes, definition) == NULL) {
            fprintf(stderr, "missing definition %s in %s\n", definition,
                capture.bytes);
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "auto encoder hard=%d stage=%d count=%u alloc=%d: %s\n",
            hard, stage, view.entry_count, auto_cover_allocations,
            sixel_helper_get_additional_message());
    }
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
