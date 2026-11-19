/* SPDX-License-Identifier: MIT */
/*
 * A decoder object keeps its parallel decode workers between calls.  Repeated
 * decodes through the object, including the bare-body entry, must match a
 * decode with per-call threads, and must not grow the kept set once it holds
 * the policy's worker count.  Concurrent decodes on one object (only one of
 * them can own the kept set) and a gather launch failure (serial fallback)
 * must leave the set reusable for the next call.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/compat_stub.h"
#include "src/decoder.h"
#include "src/decoder-parallel.h"
#include "src/threading.h"

#define RESIDENT_BANDS 80
#define RESIDENT_ROUNDS 6
#define RESIDENT_RACERS 3

static char g_stream[65536];
static size_t g_stream_size;
static unsigned char *g_expected;
static size_t g_expected_size;

/* Every band carries a carriage return so threaded gather has work. */
static size_t
resident_build(char *out, size_t capacity)
{
    size_t used;
    int band;
    int n;

    n = snprintf(out, capacity, "\033Pq\"1;1;64;%d#1;2;100;0;0#2;2;0;0;100",
                 RESIDENT_BANDS * 6);
    if (n < 0 || (size_t)n >= capacity) {
        return 0u;
    }
    used = (size_t)n;
    for (band = 0; band < RESIDENT_BANDS; ++band) {
        n = snprintf(out + used, capacity - used, "#1!64F$#2!%dw%s",
                     16 + band % 40, band + 1 < RESIDENT_BANDS ? "-" : "");
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

static int
resident_matches(sixel_decode_result_t *result, char const *label)
{
    int ok;

    ok = result->pixels != NULL &&
         (size_t)result->stride * (size_t)result->height == g_expected_size &&
         memcmp(result->pixels, g_expected, g_expected_size) == 0;
    if (!ok) {
        fprintf(stderr, "%s: pixels differ from the per-call decode\n", label);
    }
    free(result->pixels);
    result->pixels = NULL;
    return ok;
}

static int
resident_decode(sixel_decoder_t *decoder, int body, char const *label)
{
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    SIXELSTATUS status;

    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    if (body) {
        /* Strip "ESC P q" and the string terminator. */
        status = sixel_decoder_decode_body_mapped(
            decoder, (unsigned char const *)g_stream + 3, g_stream_size - 5u,
            NULL, 0u, &options, NULL, &result);
    } else {
        status = sixel_decoder_decode_mapped(
            decoder, (unsigned char const *)g_stream, g_stream_size,
            &options, NULL, &result);
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: decode failed\n", label);
        return 0;
    }
    return resident_matches(&result, label);
}

typedef struct resident_racer {
    sixel_decoder_t *decoder;
    int failed;
} resident_racer_t;

static int
resident_racer(void *arg)
{
    resident_racer_t *racer;
    int round;

    racer = (resident_racer_t *)arg;
    for (round = 0; round < RESIDENT_ROUNDS; ++round) {
        if (!resident_decode(racer->decoder, round & 1, "racer")) {
            racer->failed = 1;
            return 1;
        }
    }
    return 0;
}

int
test_decoder_0111(int argc, char **argv)
{
    sixel_decoder_t *decoder;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    sixel_thread_t threads[RESIDENT_RACERS];
    resident_racer_t racers[RESIDENT_RACERS];
    SIXELSTATUS status;
    int kept;
    int round;
    int started;
    int i;
    int ok;

    (void)argc;
    (void)argv;
    decoder = NULL;
    g_expected = NULL;
    started = 0;
    ok = 0;

    g_stream_size = resident_build(g_stream, sizeof(g_stream));
    if (g_stream_size == 0u) {
        goto end;
    }

    /* Reference: the process budget, with threads created per call. */
    sixel_set_decoder_threads(4);
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    status = sixel_decode_pixels((unsigned char const *)g_stream,
                                 g_stream_size, &options, &result, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "reference decode failed\n");
        goto end;
    }
    g_expected = result.pixels;
    g_expected_size = (size_t)result.stride * (size_t)result.height;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_THREADING_POLICY,
                                  "greedy:B4");
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (sixel_decoder_resident_threads(decoder->resident) != 0) {
        fprintf(stderr, "a new decoder already holds threads\n");
        goto end;
    }

    if (!resident_decode(decoder, 0, "first")) {
        goto end;
    }
    kept = sixel_decoder_resident_threads(decoder->resident);
    if (kept < 2) {
        fprintf(stderr, "the first decode kept %d threads\n", kept);
        goto end;
    }
    for (round = 0; round < RESIDENT_ROUNDS; ++round) {
        if (!resident_decode(decoder, round & 1, "repeat")) {
            goto end;
        }
    }
    if (sixel_decoder_resident_threads(decoder->resident) != kept) {
        fprintf(stderr, "repeated decodes grew the set from %d to %d\n",
                kept, sixel_decoder_resident_threads(decoder->resident));
        goto end;
    }

    for (i = 0; i < RESIDENT_RACERS; ++i) {
        racers[i].decoder = decoder;
        racers[i].failed = 0;
        if (sixel_thread_create(&threads[i], resident_racer,
                                &racers[i]) != 0) {
            break;
        }
        started += 1;
    }
    for (i = 0; i < started; ++i) {
        sixel_thread_join(&threads[i]);
    }
    if (started != RESIDENT_RACERS) {
        goto end;
    }
    for (i = 0; i < started; ++i) {
        if (racers[i].failed) {
            goto end;
        }
    }
    if (sixel_decoder_resident_threads(decoder->resident) != kept) {
        fprintf(stderr, "concurrent decodes grew the set\n");
        goto end;
    }

    /* Gather launch failure falls back to the serial decode; the workers
     * already handed gather jobs must be idle again for the next call. */
    if (sixel_compat_setenv("_SIXEL_TEST_DECODER_PARALLEL_FAILURE",
                            "gather:1") != 0) {
        goto end;
    }
    i = resident_decode(decoder, 0, "gather failure");
    (void)sixel_compat_setenv("_SIXEL_TEST_DECODER_PARALLEL_FAILURE", "");
    if (!i || !resident_decode(decoder, 1, "after gather failure")) {
        goto end;
    }
    ok = 1;

end:
    if (decoder != NULL) {
        sixel_decoder_unref(decoder);
    }
    sixel_set_decoder_threads(1);
    free(g_expected);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
