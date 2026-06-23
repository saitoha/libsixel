/*
 * SPDX-License-Identifier: MIT
 *
 * Verify fused fast4 decode+undither output matches scalar fast4 output.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/decoder.h"
#include "src/decoder-image.h"
#include "src/decoder-parallel.h"
#include "src/threading.h"

#define KUNDITHER_FUSED_WIDTH 96
#define KUNDITHER_FUSED_HEIGHT 72
#define KUNDITHER_FUSED_COLORS 32
#define KUNDITHER_FUSED_ACTIVE_COLORS 256
#define KUNDITHER_FUSED_PIXELS \
    (KUNDITHER_FUSED_WIDTH * KUNDITHER_FUSED_HEIGHT)

static int
kundither_fused_color_at(int x, int y)
{
    int value;

    value = x * 13 + y * 7 + ((x >> 3) ^ (y >> 2)) * 5;
    return value % KUNDITHER_FUSED_COLORS;
}

static void
kundither_fused_init_palette(int *packed, unsigned char *rgb)
{
    int i;
    int r;
    int g;
    int b;
    int color;

    for (i = 0; i < KUNDITHER_FUSED_ACTIVE_COLORS; ++i) {
        color = packed[i];
        rgb[i * 3 + 0] = (unsigned char)((color >> 16) & 255);
        rgb[i * 3 + 1] = (unsigned char)((color >> 8) & 255);
        rgb[i * 3 + 2] = (unsigned char)(color & 255);
    }

    for (i = 0; i < KUNDITHER_FUSED_COLORS; ++i) {
        r = (i * 43 + 17) & 255;
        g = (i * 73 + 29) & 255;
        b = (i * 109 + 13) & 255;
        packed[i] = (r << 16) | (g << 8) | b;
        rgb[i * 3 + 0] = (unsigned char)r;
        rgb[i * 3 + 1] = (unsigned char)g;
        rgb[i * 3 + 2] = (unsigned char)b;
    }
}

static void
kundither_fused_init_indexed(unsigned char *indexed)
{
    int x;
    int y;

    for (y = 0; y < KUNDITHER_FUSED_HEIGHT; ++y) {
        for (x = 0; x < KUNDITHER_FUSED_WIDTH; ++x) {
            indexed[y * KUNDITHER_FUSED_WIDTH + x] =
                (unsigned char)kundither_fused_color_at(x, y);
        }
    }
}

static int
kundither_fused_append_byte(unsigned char **payload,
                            size_t *length,
                            size_t *capacity,
                            int byte,
                            sixel_allocator_t *allocator)
{
    unsigned char *grown;
    size_t new_capacity;

    if (*length + 1u > *capacity) {
        new_capacity = *capacity * 2u;
        if (new_capacity < 1024u) {
            new_capacity = 1024u;
        }
        grown = (unsigned char *)sixel_allocator_realloc(
            allocator,
            *payload,
            new_capacity);
        if (grown == NULL) {
            return 0;
        }
        *payload = grown;
        *capacity = new_capacity;
    }

    (*payload)[*length] = (unsigned char)byte;
    *length += 1u;
    return 1;
}

static int
kundither_fused_append_text(unsigned char **payload,
                            size_t *length,
                            size_t *capacity,
                            char const *text,
                            sixel_allocator_t *allocator)
{
    char const *cursor;

    cursor = text;
    while (*cursor != '\0') {
        if (!kundither_fused_append_byte(payload,
                                         length,
                                         capacity,
                                         (unsigned char)*cursor,
                                         allocator)) {
            return 0;
        }
        ++cursor;
    }

    return 1;
}

static int
kundither_fused_append_number(unsigned char **payload,
                              size_t *length,
                              size_t *capacity,
                              int value,
                              sixel_allocator_t *allocator)
{
    char text[32];

    (void)snprintf(text, sizeof(text), "%d", value);
    return kundither_fused_append_text(payload,
                                       length,
                                       capacity,
                                       text,
                                       allocator);
}

static int
kundither_fused_make_payload(unsigned char **payload,
                             int *payload_len,
                             sixel_allocator_t *allocator)
{
    unsigned char *data;
    size_t length;
    size_t capacity;
    int band;
    int color;
    int x;
    int row;
    int bits;

    data = NULL;
    length = 0u;
    capacity = 0u;

    /*
     * sixel_decoder_parallel_request_start() is entered after the serial
     * parser has already consumed DCS and raster attributes, so this fixture
     * starts directly at the stable sixel body.
     */
    for (band = 0; band < KUNDITHER_FUSED_HEIGHT; band += 6) {
        for (color = 0; color < KUNDITHER_FUSED_COLORS; ++color) {
            if (!kundither_fused_append_byte(&data, &length, &capacity,
                                             '#', allocator)) {
                goto error;
            }
            if (!kundither_fused_append_number(&data, &length, &capacity,
                                               color, allocator)) {
                goto error;
            }
            for (x = 0; x < KUNDITHER_FUSED_WIDTH; ++x) {
                bits = 0;
                for (row = 0; row < 6; ++row) {
                    if (kundither_fused_color_at(x, band + row) ==
                            color) {
                        bits |= 1 << row;
                    }
                }
                if (!kundither_fused_append_byte(&data,
                                                 &length,
                                                 &capacity,
                                                 '?' + bits,
                                                 allocator)) {
                    goto error;
                }
            }
            if (color + 1 < KUNDITHER_FUSED_COLORS &&
                    !kundither_fused_append_byte(&data,
                                                 &length,
                                                 &capacity,
                                                 '$',
                                                 allocator)) {
                goto error;
            }
        }
        if (band + 6 < KUNDITHER_FUSED_HEIGHT &&
                !kundither_fused_append_byte(&data,
                                             &length,
                                             &capacity,
                                             '-',
                                             allocator)) {
            goto error;
        }
    }
    *payload = data;
    *payload_len = (int)length;
    return 1;

error:
    sixel_allocator_free(allocator, data);
    return 0;
}

static int
kundither_fused_compare_indexed(unsigned char const *expected,
                                unsigned char const *actual)
{
    size_t i;

    for (i = 0u; i < KUNDITHER_FUSED_PIXELS; ++i) {
        if (expected[i] != actual[i]) {
            fprintf(stderr,
                    "indexed byte %lu is %u, expected %u\n",
                    (unsigned long)i,
                    (unsigned int)actual[i],
                    (unsigned int)expected[i]);
            return 0;
        }
    }

    return 1;
}

static int
kundither_fused_build_reference(unsigned char const *indexed,
                                unsigned char const *palette,
                                sixel_allocator_t *allocator,
                                unsigned char **reference)
{
    unsigned char *rgba;
    unsigned char *local_indexed;
    unsigned char *local_rgb;
    int start;
    int end;
    int decode_start;
    int body_start;
    int local_rows;
    int x;
    int y;
    int src_row;
    int status;

    rgba = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)KUNDITHER_FUSED_PIXELS * 4u);
    if (rgba == NULL) {
        return 0;
    }
    local_indexed = NULL;
    local_rgb = NULL;

    for (y = 0; y < KUNDITHER_FUSED_HEIGHT; ++y) {
        for (x = 0; x < KUNDITHER_FUSED_WIDTH; ++x) {
            rgba[((size_t)y * KUNDITHER_FUSED_WIDTH + x) * 4u + 0u] =
                palette[0];
            rgba[((size_t)y * KUNDITHER_FUSED_WIDTH + x) * 4u + 1u] =
                palette[1];
            rgba[((size_t)y * KUNDITHER_FUSED_WIDTH + x) * 4u + 2u] =
                palette[2];
            rgba[((size_t)y * KUNDITHER_FUSED_WIDTH + x) * 4u + 3u] =
                255u;
        }
    }

    /*
     * The synthetic payload is uniform enough that four byte spans map to
     * four 18-row body ranges.  Rebuild the same one-band top halo semantics
     * in scalar form so the test locks the fused mode, not full-image fast4.
     */
    for (start = 0; start < KUNDITHER_FUSED_HEIGHT; start += 18) {
        end = start + 18;
        if (end > KUNDITHER_FUSED_HEIGHT) {
            end = KUNDITHER_FUSED_HEIGHT;
        }
        decode_start = start >= 6 ? start - 6 : 0;
        body_start = start - decode_start;
        local_rows = end - decode_start;
        local_indexed = (unsigned char *)sixel_allocator_malloc(
            allocator,
            (size_t)local_rows * KUNDITHER_FUSED_WIDTH);
        local_rgb = (unsigned char *)sixel_allocator_malloc(
            allocator,
            (size_t)local_rows * KUNDITHER_FUSED_WIDTH * 3u);
        if (local_indexed == NULL || local_rgb == NULL) {
            goto error;
        }
        for (y = 0; y < local_rows; ++y) {
            src_row = decode_start + y;
            memcpy(local_indexed + (size_t)y * KUNDITHER_FUSED_WIDTH,
                   indexed + (size_t)src_row * KUNDITHER_FUSED_WIDTH,
                   KUNDITHER_FUSED_WIDTH);
        }
        status = sixel_dequantize_k_undither_fast4_rows(
            local_indexed,
            KUNDITHER_FUSED_WIDTH,
            local_rows,
            palette,
            KUNDITHER_FUSED_ACTIVE_COLORS,
            100,
            body_start,
            local_rows,
            allocator,
            local_rgb);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
        for (y = body_start; y < local_rows; ++y) {
            src_row = decode_start + y;
            for (x = 0; x < KUNDITHER_FUSED_WIDTH; ++x) {
                rgba[((size_t)src_row * KUNDITHER_FUSED_WIDTH + x) *
                     4u + 0u] =
                    local_rgb[((size_t)y * KUNDITHER_FUSED_WIDTH + x) *
                              3u + 0u];
                rgba[((size_t)src_row * KUNDITHER_FUSED_WIDTH + x) *
                     4u + 1u] =
                    local_rgb[((size_t)y * KUNDITHER_FUSED_WIDTH + x) *
                              3u + 1u];
                rgba[((size_t)src_row * KUNDITHER_FUSED_WIDTH + x) *
                     4u + 2u] =
                    local_rgb[((size_t)y * KUNDITHER_FUSED_WIDTH + x) *
                              3u + 2u];
            }
        }
        sixel_allocator_free(allocator, local_rgb);
        sixel_allocator_free(allocator, local_indexed);
        local_rgb = NULL;
        local_indexed = NULL;
    }

    *reference = rgba;
    return 1;

error:
    sixel_allocator_free(allocator, local_rgb);
    sixel_allocator_free(allocator, local_indexed);
    sixel_allocator_free(allocator, rgba);
    return 0;
}

static int
kundither_fused_compare(unsigned char const *expected,
                        unsigned char const *actual)
{
    size_t pixel;
    size_t index;

    for (pixel = 0u; pixel < KUNDITHER_FUSED_PIXELS; ++pixel) {
        index = pixel * 4u;
        if (expected[index + 0u] != actual[index + 0u] ||
                expected[index + 1u] != actual[index + 1u] ||
                expected[index + 2u] != actual[index + 2u] ||
                expected[index + 3u] != actual[index + 3u]) {
            fprintf(stderr,
                    "pixel %lu differs: actual=%u,%u,%u,%u "
                    "expected=%u,%u,%u,%u\n",
                    (unsigned long)pixel,
                    (unsigned int)actual[index + 0u],
                    (unsigned int)actual[index + 1u],
                    (unsigned int)actual[index + 2u],
                    (unsigned int)actual[index + 3u],
                    (unsigned int)expected[index + 0u],
                    (unsigned int)expected[index + 1u],
                    (unsigned int)expected[index + 2u],
                    (unsigned int)expected[index + 3u]);
            return 0;
        }
    }

    return 1;
}

int
test_decoder_0015_decoder_kundither_fast4_fused_matches_scalar(
    int argc,
    char **argv)
{
    sixel_allocator_t *allocator;
    image_buffer_t *image;
    image_buffer_t *plain_image;
    sixel_decoder_undither_context_t undither;
    unsigned char *payload;
    unsigned char *indexed;
    unsigned char *reference_rgba;
    unsigned char palette_rgb[KUNDITHER_FUSED_ACTIVE_COLORS * 3];
    int payload_len;
    int painted_outside;
    int success;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;

    allocator = NULL;
    image = NULL;
    plain_image = NULL;
    payload = NULL;
    indexed = NULL;
    reference_rgba = NULL;
    payload_len = 0;
    painted_outside = 0;
    success = 0;
    memset(&undither, 0, sizeof(undither));

    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) !=
            SIXEL_OK) {
        fprintf(stderr, "allocator setup failed\n");
        goto end;
    }
    indexed = (unsigned char *)sixel_allocator_malloc(
        allocator,
        KUNDITHER_FUSED_PIXELS);
    if (indexed == NULL) {
        fprintf(stderr, "indexed fixture allocation failed\n");
        goto end;
    }
    image = (image_buffer_t *)malloc(sizeof(*image));
    if (image == NULL) {
        fprintf(stderr, "image allocation failed\n");
        goto end;
    }
    memset(image, 0, sizeof(*image));

    status = image_buffer_init(image,
                               KUNDITHER_FUSED_WIDTH,
                               KUNDITHER_FUSED_HEIGHT,
                               0,
                               1,
                               0,
                               allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "image setup failed: %04x\n", status);
        goto end;
    }

    kundither_fused_init_palette(image->palette, palette_rgb);
    kundither_fused_init_indexed(indexed);

    sixel_set_threads(1);
    if (!kundither_fused_build_reference(
        indexed,
        palette_rgb,
        allocator,
        &reference_rgba)) {
        fprintf(stderr, "scalar fused reference failed\n");
        goto end;
    }
    if (!kundither_fused_make_payload(&payload, &payload_len, allocator)) {
        fprintf(stderr, "payload setup failed\n");
        goto end;
    }

    plain_image = (image_buffer_t *)malloc(sizeof(*plain_image));
    if (plain_image == NULL) {
        fprintf(stderr, "plain image allocation failed\n");
        goto end;
    }
    memset(plain_image, 0, sizeof(*plain_image));
    status = image_buffer_init(plain_image,
                               KUNDITHER_FUSED_WIDTH,
                               KUNDITHER_FUSED_HEIGHT,
                               0,
                               1,
                               0,
                               allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "plain image setup failed: %04x\n", status);
        goto end;
    }
    kundither_fused_init_palette(plain_image->palette, palette_rgb);
    status = sixel_decoder_parallel_override_threads("4");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "plain thread override failed: %04x\n", status);
        goto end;
    }
    status = sixel_decoder_parallel_request_start(
        0,
        0,
        payload,
        payload_len,
        payload,
        plain_image,
        0,
        plain_image->palette,
        NULL,
        0U,
        &painted_outside,
        NULL);
    if (status != SIXEL_OK) {
        fprintf(stderr, "plain request failed: %04x\n", status);
        goto end;
    }
    if (!kundither_fused_compare_indexed(indexed,
                                         plain_image->pixels.in_bytes)) {
        goto end;
    }

    undither.enabled = 1;
    undither.direct_output = 1;
    undither.similarity_bias = 100;
    undither.allocator = allocator;
    status = sixel_decoder_parallel_override_threads("4");
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "thread override failed: %04x\n", status);
        goto end;
    }
    status = sixel_decoder_parallel_request_start(
        0,
        0,
        payload,
        payload_len,
        payload,
        image,
        0,
        image->palette,
        NULL,
        0U,
        &painted_outside,
        &undither);
    if (status != SIXEL_OK) {
        fprintf(stderr, "fused request failed: %04x\n", status);
        goto end;
    }
    if (painted_outside) {
        fprintf(stderr, "fused request painted outside raster\n");
        goto end;
    }
    if (!kundither_fused_compare(reference_rgba, undither.pixels)) {
        goto end;
    }

    success = 1;

end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, undither.pixels);
        sixel_allocator_free(allocator, reference_rgba);
        sixel_allocator_free(allocator, indexed);
        sixel_allocator_free(allocator, payload);
        if (image != NULL) {
            sixel_allocator_free(allocator, image->pixels.p);
        }
        if (plain_image != NULL) {
            sixel_allocator_free(allocator, plain_image->pixels.p);
        }
        sixel_allocator_unref(allocator);
    }
    free(image);
    free(plain_image);
    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
