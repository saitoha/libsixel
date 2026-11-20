/* SPDX-License-Identifier: MIT */
/*
 * The decoder's final pass reports whether every RGBA alpha byte is 255, and
 * sixel_decode_pixels() publishes that as ALPHA_OPAQUE without scanning the
 * result.  Whatever path painted the image, the flag must agree with a walk
 * over the returned alpha bytes.  The streams cover holes in full and partial
 * bands, widths on both sides of the SIMD block, carriage returns, a grown
 * canvas and trusted-raster clipping; large ones give parallel gathers work.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

static int
walk_opaque(unsigned char const *pixels, int width, int height)
{
    size_t i;
    size_t total;

    total = (size_t)width * (size_t)height;
    for (i = 0; i < total; ++i) {
        if (pixels[i * 4u + 3u] != 255u) {
            return 0;
        }
    }
    return 1;
}

static int
check_stream(unsigned char const *data, size_t size, unsigned int options,
             char const *label)
{
    sixel_decode_options_t decode_options;
    sixel_decode_result_t result;
    SIXELSTATUS status;
    int flagged;
    int walked;

    memset(&decode_options, 0, sizeof(decode_options));
    memset(&result, 0, sizeof(result));
    decode_options.flags = options;
    decode_options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    status = sixel_decode_pixels(data, size, &decode_options, &result, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: decode failed\n", label);
        return 0;
    }
    flagged = (result.flags & SIXEL_DECODE_PIXELS_RESULT_ALPHA_OPAQUE) != 0u;
    walked = walk_opaque(result.pixels, result.width, result.height);
    free(result.pixels);
    if ((result.flags & 0xc0000000u) != 0u) {
        fprintf(stderr, "%s: internal result bits leaked\n", label);
        return 0;
    }
    if (flagged != walked) {
        fprintf(stderr, "%s: flag %d, walk %d\n", label, flagged, walked);
        return 0;
    }
    return 1;
}

/*
 * A tall stream with carriage returns in every band, so a threaded decode
 * splits it across gather workers.  hole_band < 0 paints everything.
 */
static size_t
build_tall(char *out, size_t capacity, int bands, int hole_band)
{
    size_t used;
    int band;
    int n;

    used = 0u;
    n = snprintf(out, capacity, "\033Pq\"1;1;64;%d#1;2;100;0;0#2;2;0;0;100",
                 bands * 6);
    if (n < 0 || (size_t)n >= capacity) {
        return 0u;
    }
    used = (size_t)n;
    for (band = 0; band < bands; ++band) {
        if (band == hole_band) {
            n = snprintf(out + used, capacity - used,
                         "#1!64F$#2!40w?!23w%s", band + 1 < bands ? "-" : "");
        } else {
            n = snprintf(out + used, capacity - used,
                         "#1!64F$#2!64w%s", band + 1 < bands ? "-" : "");
        }
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

int
test_decoder_0110(int argc, char **argv)
{
    static char const *const bodies[] = {
        /* Two full bands. */
        "\"1;1;8;12#1;2;100;0;0#1!8~-!8~",
        /* A hole in the second band. */
        "\"1;1;8;12#1;2;100;0;0#1!8~-!3~?!4~",
        /* A partial last band, fully painted and with one row missing. */
        "\"1;1;8;8#1;2;0;100;0#1!8~-!8~",
        "\"1;1;8;8#1;2;0;100;0#1!8~-!7~A",
        /* Wider than one SIMD block, full and with a hole past it. */
        "\"1;1;40;12#1;2;0;0;100#1!40~-!40~",
        "\"1;1;40;12#1;2;0;0;100#1!40~-!33~?!6~",
        /* Colors laid over each other through carriage returns. */
        "\"1;1;24;6#1;2;100;0;0#2;2;0;100;0#1!24F$#2!24w",
        "\"1;1;24;6#1;2;100;0;0#2;2;0;100;0#1!24F$#2!23w",
        /* Painting past the raster grows the canvas; the declared second band
         * stays unpainted unless trusted clipping drops the overflow. */
        "\"1;1;4;12#1;2;100;100;0#1!10~",
        "\"1;1;4;6#1;2;100;100;0#1!10~",
        /* Black paint is still paint. */
        "\"1;1;4;6#1;2;0;0;0#1!4~",
        /* Nothing painted at all. */
        "\"1;1;4;6#1;2;0;0;0#1!4?",
    };
    static char tall[65536];
    unsigned char stream[512];
    char label[64];
    unsigned int options;
    size_t i;
    size_t len;
    int trust;
    int holes[] = { -1, 0, 37, 79 };

    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i) {
        len = strlen(bodies[i]);
        if (len + 5u > sizeof(stream)) {
            return EXIT_FAILURE;
        }
        memcpy(stream, "\033Pq", 3u);
        memcpy(stream + 3, bodies[i], len);
        memcpy(stream + 3 + len, "\033\\", 2u);
        for (trust = 0; trust < 2; ++trust) {
            options = trust ? SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE : 0u;
            snprintf(label, sizeof(label), "body %u trust %d", (unsigned)i,
                     trust);
            if (!check_stream(stream, len + 5u, options, label)) {
                return EXIT_FAILURE;
            }
        }
    }
    for (i = 0; i < sizeof(holes) / sizeof(holes[0]); ++i) {
        len = build_tall(tall, sizeof(tall), 80, holes[i]);
        if (len == 0u) {
            return EXIT_FAILURE;
        }
        snprintf(label, sizeof(label), "tall hole %d", holes[i]);
        if (!check_stream((unsigned char const *)tall, len, 0u, label)) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
