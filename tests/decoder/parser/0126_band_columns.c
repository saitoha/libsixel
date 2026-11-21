/* SPDX-License-Identifier: MIT */
/*
 * Band-column output (SIXEL_PIXELFORMAT_BANDCOLUMNS) must expand to exactly
 * the RGBA8888 decode of the same stream, through both mapped entries, with
 * a palette transform, for opaque and transparent images and a final band
 * the trusted raster cuts short.  A decode that cannot keep the columns (a
 * serial decode, or an untrusted raster the paint outgrows) returns RGBA8888
 * instead, and the CPU expansion helper rejects bad arguments.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#define BAND_WIDTH 64
#define BAND_BANDS 80
#define BAND_HEIGHT (BAND_BANDS * 6 - 2)

static char g_stream[65536];
static size_t g_stream_size;
static unsigned int g_flags;

/*
 * Every band carries a carriage return so the threaded gather has work.
 * holes leaves the upper rows of the second color run unpainted.
 */
static size_t
band_build(char *out, size_t capacity, int p2, int holes)
{
    size_t used;
    int band;
    int n;

    n = snprintf(out, capacity,
                 "\033P0;%dq\"1;1;%d;%d#1;2;100;0;0#2;2;0;60;100#3;2;0;80;0",
                 p2, BAND_WIDTH, BAND_HEIGHT);
    if (n < 0 || (size_t)n >= capacity) {
        return 0u;
    }
    used = (size_t)n;
    for (band = 0; band < BAND_BANDS; ++band) {
        n = snprintf(out + used, capacity - used, "#1!%d~#3!%d~$#2!%d%c%s",
                     16 + band % 32, BAND_WIDTH - 16 - band % 32,
                     8 + band % 48, holes ? 'p' : '~',
                     band + 1 < BAND_BANDS ? "-" : "");
        if (n < 0 || (size_t)n >= capacity - used) {
            return 0u;
        }
        used += (size_t)n;
    }
    if (used + 2u >= capacity) {
        return 0u;
    }
    out[used++] = '\033';
    out[used++] = '\\';
    return used;
}

static void
band_swap_red_blue(void *context, unsigned char rgb[3], int is_default)
{
    unsigned char red;

    (void)context;
    (void)is_default;
    red = rgb[0];
    rgb[0] = rgb[2];
    rgb[2] = red;
}

static SIXELSTATUS
band_decode(sixel_decoder_t *decoder, int body, int pixelformat,
            sixel_palette_transform_t const *transform,
            sixel_decode_result_t *result)
{
    sixel_decode_options_t options;
    char const *q;

    memset(&options, 0, sizeof(options));
    memset(result, 0, sizeof(*result));
    options.preferred_pixelformat = pixelformat;
    options.flags = g_flags;
    if (body) {
        /* Strip the introducer through 'q' and the string terminator. */
        q = strchr(g_stream, 'q');
        return sixel_decoder_decode_body_mapped(
            decoder, (unsigned char const *)q + 1,
            g_stream_size - (size_t)(q + 1 - g_stream) - 2u, NULL, 0u,
            &options, transform, result);
    }
    return sixel_decoder_decode_mapped(
        decoder, (unsigned char const *)g_stream, g_stream_size, &options,
        transform, result);
}

/* Decodes both forms and requires the columns to expand to the RGBA bytes. */
static int
band_check(sixel_decoder_t *decoder, int body,
           sixel_palette_transform_t const *transform, int expect_columns,
           char const *label)
{
    sixel_decode_result_t rgba;
    sixel_decode_result_t band;
    unsigned char *expanded;
    size_t bytes;
    int ok;

    ok = 0;
    expanded = NULL;
    memset(&band, 0, sizeof(band));
    if (SIXEL_FAILED(band_decode(decoder, body, SIXEL_PIXELFORMAT_RGBA8888,
                                 transform, &rgba))) {
        fprintf(stderr, "%s: RGBA decode failed\n", label);
        return 0;
    }
    if (SIXEL_FAILED(band_decode(decoder, body, SIXEL_PIXELFORMAT_BANDCOLUMNS,
                                 transform, &band))) {
        fprintf(stderr, "%s: band-column decode failed\n", label);
        goto end;
    }
    if (band.width != rgba.width || band.height != rgba.height ||
            band.flags != rgba.flags) {
        fprintf(stderr, "%s: geometry or flags differ\n", label);
        goto end;
    }
    bytes = (size_t)rgba.width * (size_t)rgba.height * 4u;
    if (band.pixelformat == SIXEL_PIXELFORMAT_RGBA8888) {
        if (expect_columns || memcmp(band.pixels, rgba.pixels, bytes) != 0) {
            fprintf(stderr, "%s: unexpected RGBA fallback\n", label);
            goto end;
        }
        ok = 1;
        goto end;
    }
    if (!expect_columns || band.pixelformat != SIXEL_PIXELFORMAT_BANDCOLUMNS ||
            band.stride != band.width * 8) {
        fprintf(stderr, "%s: unexpected band-column result\n", label);
        goto end;
    }
    /* Seed with nonzero bytes so a skipped pixel cannot pass as zero. */
    expanded = (unsigned char *)malloc(bytes);
    if (expanded == NULL) {
        goto end;
    }
    memset(expanded, 0x5a, bytes);
    if (SIXEL_FAILED(sixel_bandcolumns_to_rgba(
            band.pixels, band.width, band.height, expanded,
            (size_t)band.width * 4u, 0))) {
        fprintf(stderr, "%s: expansion failed\n", label);
        goto end;
    }
    if (memcmp(expanded, rgba.pixels, bytes) != 0) {
        fprintf(stderr, "%s: expanded columns differ from RGBA\n", label);
        goto end;
    }
    ok = 1;

end:
    free(expanded);
    free(rgba.pixels);
    free(band.pixels);
    return ok;
}

/* keep_unpainted leaves unpainted destination bytes alone. */
static int
band_check_keep(sixel_decoder_t *decoder)
{
    sixel_decode_result_t band;
    unsigned char *kept;
    unsigned char *cleared;
    size_t bytes;
    size_t i;
    int ok;

    ok = 0;
    kept = NULL;
    cleared = NULL;
    if (SIXEL_FAILED(band_decode(decoder, 0, SIXEL_PIXELFORMAT_BANDCOLUMNS,
                                 NULL, &band)) ||
            band.pixelformat != SIXEL_PIXELFORMAT_BANDCOLUMNS) {
        fprintf(stderr, "keep: no band columns\n");
        free(band.pixels);
        return 0;
    }
    bytes = (size_t)band.width * (size_t)band.height * 4u;
    kept = (unsigned char *)malloc(bytes);
    cleared = (unsigned char *)malloc(bytes);
    if (kept == NULL || cleared == NULL) {
        goto end;
    }
    memset(kept, 0x5a, bytes);
    if (SIXEL_FAILED(sixel_bandcolumns_to_rgba(band.pixels, band.width,
                                               band.height, kept,
                                               (size_t)band.width * 4u, 1)) ||
            SIXEL_FAILED(sixel_bandcolumns_to_rgba(
                band.pixels, band.width, band.height, cleared,
                (size_t)band.width * 4u, 0))) {
        goto end;
    }
    for (i = 0u; i < bytes; i += 4u) {
        if (cleared[i + 3u] != 0u
                ? memcmp(kept + i, cleared + i, 4u) != 0
                : (kept[i] != 0x5a || kept[i + 3u] != 0x5a)) {
            fprintf(stderr, "keep: pixel %lu mismatched\n",
                    (unsigned long)(i / 4u));
            goto end;
        }
    }
    ok = 1;

end:
    free(kept);
    free(cleared);
    free(band.pixels);
    return ok;
}

int
test_decoder_0112(int argc, char **argv)
{
    sixel_decoder_t *threaded;
    sixel_decoder_t *serial;
    sixel_palette_transform_t transform;
    unsigned char word[SIXEL_BANDCOLUMNS_PALETTE_BYTES + 8u];
    unsigned char rgba[4];
    int ok;

    (void)argc;
    (void)argv;
    threaded = NULL;
    serial = NULL;
    ok = 0;
    transform.context = NULL;
    transform.map_rgb = band_swap_red_blue;
    g_flags = SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE;

    /* Only threaded gathers keep columns; the budget caps every policy. */
    sixel_set_decoder_threads(4);
    if (SIXEL_FAILED(sixel_decoder_new(&threaded, NULL)) ||
            SIXEL_FAILED(sixel_decoder_setopt(
                threaded, SIXEL_OPTFLAG_THREADING_POLICY, "greedy:B4")) ||
            SIXEL_FAILED(sixel_decoder_new(&serial, NULL)) ||
            SIXEL_FAILED(sixel_decoder_setopt(
                serial, SIXEL_OPTFLAG_THREADING_POLICY, "greedy:B1"))) {
        goto end;
    }

    /* Opaque, then transparent with holes, through both entries. */
    g_stream_size = band_build(g_stream, sizeof(g_stream), 0, 0);
    if (g_stream_size == 0u ||
            !band_check(threaded, 0, NULL, 1, "opaque stream") ||
            !band_check(threaded, 1, NULL, 1, "opaque body") ||
            !band_check(threaded, 0, &transform, 1, "opaque transform") ||
            !band_check(serial, 0, NULL, 0, "opaque serial")) {
        goto end;
    }
    /* Untrusted, the last band paints past the raster and grows the image. */
    g_flags = 0u;
    if (!band_check(threaded, 0, NULL, 0, "grown raster")) {
        goto end;
    }
    g_flags = SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE;
    g_stream_size = band_build(g_stream, sizeof(g_stream), 1, 1);
    if (g_stream_size == 0u ||
            !band_check(threaded, 0, NULL, 1, "transparent stream") ||
            !band_check(threaded, 1, &transform, 1, "transparent body") ||
            !band_check_keep(threaded)) {
        goto end;
    }

    if (sixel_bandcolumns_to_rgba(NULL, 1, 1, rgba, 4u, 0) !=
                SIXEL_BAD_ARGUMENT ||
            sixel_bandcolumns_to_rgba(word, 1, 1, NULL, 4u, 0) !=
                SIXEL_BAD_ARGUMENT ||
            sixel_bandcolumns_to_rgba(word, 0, 1, rgba, 4u, 0) !=
                SIXEL_BAD_ARGUMENT ||
            sixel_bandcolumns_to_rgba(word, 2, 1, rgba, 4u, 0) !=
                SIXEL_BAD_ARGUMENT) {
        fprintf(stderr, "expansion accepted a bad argument\n");
        goto end;
    }
    ok = 1;

end:
    if (threaded != NULL) {
        sixel_decoder_unref(threaded);
    }
    if (serial != NULL) {
        sixel_decoder_unref(serial);
    }
    sixel_set_decoder_threads(1);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
