/* SPDX-License-Identifier: MIT */
/* Compare host I420/NV12 conversion plus bilinear resize on the same input.
 * Build against a configured shared library:
 * cc -O2 -I"$build/include" tools/bench/ycbcr420-resize.c \
 *   -L"$build/src/.libs" -Wl,-rpath,"$build/src/.libs" -lsixel -lm -o bench
 * Run ./bench [samples] with the same CPU budget for both routes.
 * Timing excludes capture, frame setup and SIXEL encoding. Peak bytes
 * count live allocations through the frame allocator above setup. */
#define _POSIX_C_SOURCE 200809L
#include <sixel.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { WIDTH = 1920, HEIGHT = 1080, WARMUP = 4, MAX_SAMPLES = 500 };

typedef union allocation {
    size_t bytes;
    long double alignment;
    void *pointer_alignment;
} allocation_t;

static size_t live, peak, calls;

static void *
bench_malloc(size_t bytes)
{
    allocation_t *entry = malloc(sizeof(*entry) + bytes);

    if (entry != NULL) {
        entry->bytes = bytes;
        live += bytes;
        if (live > peak) {
            peak = live;
        }
        ++calls;
        return entry + 1;
    }
    return NULL;
}

static void
bench_free(void *ptr)
{
    allocation_t *entry;

    if (ptr != NULL) {
        entry = (allocation_t *)ptr - 1;
        live -= entry->bytes;
        free(entry);
    }
}

static void *
bench_calloc(size_t n, size_t bytes)
{
    void *ptr = bench_malloc(n * bytes);

    if (ptr != NULL) {
        memset(ptr, 0, n * bytes);
    }
    return ptr;
}

static void *
bench_realloc(void *ptr, size_t bytes)
{
    void *replacement;
    size_t old_bytes;

    if (ptr == NULL) {
        return bench_malloc(bytes);
    }
    replacement = bench_malloc(bytes);
    if (replacement != NULL) {
        old_bytes = ((allocation_t *)ptr - 1)->bytes;
        memcpy(replacement, ptr, old_bytes < bytes ? old_bytes : bytes);
        bench_free(ptr);
    }
    return replacement;
}

static double
now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000.0 * ts.tv_sec + ts.tv_nsec / 1e6;
}

static int
compare_ms(void const *a, void const *b)
{
    double x = *(double const *)a, y = *(double const *)b;

    return (x > y) - (x < y);
}

static int
bench_resize(sixel_ycbcr_frame_view_t const *view, sixel_allocator_t *allocator,
             int width, int height, int samples)
{
    double times[2][MAX_SAMPLES], start, elapsed;
    size_t peaks[2] = {0}, allocations[2] = {0};
    size_t i, count, base;
    sixel_frame_t *frame[2] = {NULL, NULL};
    float const *legacy, *fused;
    float error, max_error = 0;
    int run, pass, mode;
    char const *format = view->plane_count == 2 ? "nv12" : "i420";

    count = (size_t)width * height * 3u;
    for (run = -WARMUP; run < samples; ++run) {
        for (pass = 0; pass < 2; ++pass) {
            /* Alternate the order to reduce cache and thermal bias. */
            mode = (run + WARMUP + pass) % 2;
            if (sixel_frame_new(&frame[mode], allocator) ||
                sixel_frame_init_ycbcr_borrowed(frame[mode], view)) {
                goto fail;
            }
            base = peak = live;
            calls = 0;
            start = now_ms();
            if ((mode == 0 && sixel_frame_set_pixelformat(frame[mode],
                                          SIXEL_PIXELFORMAT_RGBFLOAT32)) ||
                sixel_frame_resize_float32(frame[mode], width, height,
                                            SIXEL_RES_BILINEAR)) {
                goto fail;
            }
            elapsed = now_ms() - start;
            if (run >= 0) {
                times[mode][run] = elapsed;
                if (peak - base > peaks[mode]) {
                    peaks[mode] = peak - base;
                }
                allocations[mode] += calls;
            }
        }
        if (run == 0) {
            legacy = (float const *)(void *)sixel_frame_get_pixels(frame[0]);
            fused = (float const *)(void *)sixel_frame_get_pixels(frame[1]);
            for (i = 0; i < count; ++i) {
                error = fabsf(legacy[i] - fused[i]);
                if (!(error <= 1e-6f)) {
                    fprintf(stderr, "%s %dx%d sample %zu differs: %g\n",
                            format, width, height, i, error);
                    goto fail;
                }
                if (error > max_error) {
                    max_error = error;
                }
            }
        }
        sixel_frame_unref(frame[0]);
        sixel_frame_unref(frame[1]);
        frame[0] = frame[1] = NULL;
    }
    for (mode = 0; mode < 2; ++mode) {
        qsort(times[mode], samples, sizeof(double), compare_ms);
        printf("%s,%dx%d,%s,%d,%.3f,%.3f,%.2f,%zu,%.9g\n", format,
            width, height, mode ? "fused" : "separate", samples,
            times[mode][samples / 2], times[mode][(samples * 95 + 99)/100 - 1],
            (double)allocations[mode] / samples, peaks[mode], max_error);
    }
    return 1;
fail:
    sixel_frame_unref(frame[0]);
    sixel_frame_unref(frame[1]);
    return 0;
}

int
main(int argc, char **argv)
{
    size_t full = (size_t)WIDTH * HEIGHT;
    size_t chroma = full / 4u;
    unsigned char *data = NULL;
    sixel_color_def_t *rgb = NULL;
    sixel_ycbcr_def_t *definition = NULL;
    sixel_allocator_t *allocator = NULL;
    sixel_ycbcr_def_config_t color = {sizeof(color), SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_LIMITED, 0, 1, NULL};
    sixel_ycbcr_frame_view_t view = {sizeof(view), SIXEL_PIXELFORMAT_YUV420P,
        WIDTH, HEIGHT, 3, {{0}}, NULL};
    int samples = argc == 2 ? atoi(argv[1]) : 32;
    int format, factor, pass = 0;
    size_t i;

    if (argc > 2 || samples < 1 || samples > MAX_SAMPLES) {
        fprintf(stderr, "usage: %s [1..%d samples]\n", argv[0], MAX_SAMPLES);
        return EXIT_FAILURE;
    }
    data = malloc(full + 2u * chroma);
    rgb = sixel_color_def_new_named("srgb");
    color.rgb = rgb;
    definition = sixel_ycbcr_def_new(&color);
    if (data == NULL || definition == NULL ||
        sixel_allocator_new(&allocator, bench_malloc, bench_calloc,
                            bench_realloc, bench_free)) {
        goto end;
    }
    for (i = 0; i < full; ++i) {
        data[i] = (unsigned char)(i * 13 + i / WIDTH * 7);
    }
    view.planes[0] = (sixel_frame_plane_view_t){data, full, 0, WIDTH};
    view.definition = definition;
    puts("format,size,route,samples,median_ms,p95_ms,allocations,peak_bytes,"
         "max_rgb_error");
    for (format = 0; format < 2; ++format) {
        for (i = 0; i < chroma; ++i) {
            data[full + (format ? 2u * i : i)] = (unsigned char)(80 + i % 79);
            data[full + (format ? 2u * i + 1 : chroma + i)] =
                (unsigned char)(168 - i % 73);
        }
        if (format) {
            view.pixelformat = SIXEL_PIXELFORMAT_NV12;
            view.plane_count = 2;
            view.planes[1] = (sixel_frame_plane_view_t){
                data + full, 2u * chroma, 0, WIDTH};
        } else {
            view.planes[1] = (sixel_frame_plane_view_t){
                data + full, chroma, 0, WIDTH / 2};
            view.planes[2] = (sixel_frame_plane_view_t){
                data + full + chroma, chroma, 0, WIDTH / 2};
        }
        for (factor = 1; factor <= 3; ++factor) {
            if (!bench_resize(&view, allocator, WIDTH / factor, HEIGHT / factor,
                              samples)) {
                goto end;
            }
        }
    }
    pass = 1;
end:
    if (!pass) {
        fprintf(stderr, "benchmark failed: %s\n",
                sixel_helper_get_additional_message());
    }
    sixel_allocator_unref(allocator);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    free(data);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
