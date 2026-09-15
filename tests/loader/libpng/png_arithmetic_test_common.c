/* SPDX-License-Identifier: MIT */
/* Shared setup for individually registered arithmetic observations. The
 * literal reference values come from an independent rational Gaussian solve,
 * not the implementation's coefficients or a computed tolerance envelope. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/png-chrm-arithmetic.h"
#include "src/chunk-factory.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-common.h"
#include "tests/data/color-numeric/png-arithmetic.h"
#include "png_arithmetic_test_common.h"

int
png_arith_dispatch(int sample, int requested)
{
    static int64_t const grids[][8] = {
        {31270,32900,20000,20000,21000,20001,20001,20100},
        {31270,32900,20000,20000,21000,20000,20000,20100},
        {31270,32900,20000,20000,21000,19999,20001,20100},
        {31270,32900,20000,20000,20001,20100,21000,20001},
        {31270,32900,20000,20000,20000,20100,21000,20000},
        {31270,32900,20000,20000,20001,20100,21000,19999},
        {14071,2978,64000,33000,30000,60000,15000,6000},
        {16256,5440,64000,33000,30000,60000,15000,6000},
        {49348,49240,64000,33000,30000,60000,15000,6000},
        {14071,2978,59522,6,58680,7,57839,8}
    };
    static unsigned const reasons[] = {2,8,8,2,8,8,4,8,8,6};
    sixel_png_chrm_matrix_t matrix;
    sixel_png_chrm_stats_t stats;
    float input[3] = {0.125f, 0.25f, 0.75f};
    float output[3];
    unsigned reason;
    int selected;
    int status;

    if (sample < 0 || sample >= 10 || requested < 0 || requested > 2) {
        return 1;
    }
    reason = requested ? SIXEL_PNG_CHRM_FORCED : reasons[sample];
    selected = requested ? requested : (reasons[sample] == 8 ? 1 : 2);
    status = sixel_png_chrm_build(&matrix, grids[sample], requested, 1);
    if (status == SIXEL_PNG_CHRM_UNSUPPORTED) {
        return 77;
    }
    if (status != 1 || matrix.selected != selected || matrix.reason != reason
        || matrix.exact_ready != (selected == 2)) {
        return 1;
    }
    if (sixel_png_chrm_apply(&matrix, input, output, 1, &stats) != 1 ||
        stats.identity_pixels != 0 ||
        stats.double_pixels != (size_t)(selected == 1) ||
        stats.integer_pixels != (size_t)(selected == 2)) {
        return 1;
    }
    fprintf(stderr, "PNG_ARITH|dispatch|selected=%d|reason=%u\n",
            selected, reason);
    return 0;
}

int
png_arith_values(int sample, int requested, int adapt, int exact)
{
    sixel_png_chrm_matrix_t matrix;
    float input[3];
    float output[3];
    float reference;
    uint32_t encoded[3];
    size_t i;
    size_t j;
    int status;

    if (sample < 0 || sample >= 6 || (adapt != 0 && adapt != 1)) {
        return 1;
    }
    status = sixel_png_chrm_build(&matrix, png_arith_grids[sample],
                                  requested, adapt);
    if (status == SIXEL_PNG_CHRM_UNSUPPORTED) {
        return 77;
    }
    if (status != 1) {
        return 1;
    }
    for (i = 0; i < sizeof(png_arith_inputs) / sizeof(png_arith_inputs[0]);
         ++i) {
        memcpy(input, png_arith_inputs[i], sizeof(input));
        if (sixel_png_chrm_apply(&matrix, input, output, 1, NULL) != 1) {
            return 1;
        }
        memcpy(encoded, output, sizeof(encoded));
        for (j = 0; j < 3; ++j) {
            memcpy(&reference, &png_arith_expected[sample][adapt][i][j],
                   sizeof(reference));
            /* Keep the existing ordinary-source absolute budget. A
             * production-derived forward bound is not a quality budget. */
            if ((exact && encoded[j] !=
                    png_arith_expected[sample][adapt][i][j]) ||
                (!exact && (!isfinite(output[j]) ||
                    fabs((double)output[j] - reference) > 2e-7))) {
                fprintf(stderr, "PNG_ARITH|sample=%d|v=%lu|c=%lu|"
                        "actual=%a|expected=%a\n", sample,
                        (unsigned long)i, (unsigned long)j,
                        (double)output[j], (double)reference);
                return 1;
            }
        }
    }
    return 0;
}

int
png_arith_neutral(int requested)
{
    static uint32_t const values[] = {
        0u, 0x80000000u, 1u, 0x00800000u, 0x3f000000u, 0x3f800000u
    };
    sixel_png_chrm_matrix_t matrix;
    sixel_png_chrm_stats_t stats;
    uint32_t input_bits[3];
    uint32_t output_bits[3];
    float input[3];
    float output[3];
    size_t i;
    size_t j;
    int status;

    status = sixel_png_chrm_build(&matrix, png_arith_grids[5], requested, 1);
    if (status == SIXEL_PNG_CHRM_UNSUPPORTED) {
        return 77;
    }
    if (status != 1) {
        return 1;
    }
    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        for (j = 0; j < 3; ++j) {
            input_bits[j] = values[i];
        }
        memcpy(input, input_bits, sizeof(input));
        if (sixel_png_chrm_apply(&matrix, input, output, 1, &stats) != 1) {
            return 1;
        }
        memcpy(output_bits, output, sizeof(output_bits));
        for (j = 0; j < 3; ++j) {
            if (output_bits[j] != (values[i] & 0x7fffffffu)) {
                return 1;
            }
        }
        if (stats.identity_pixels != 1 || stats.double_pixels != 0 ||
            stats.integer_pixels != 0) {
            return 1;
        }
    }
    return 0;
}

int
png_arith_target(void)
{
    sixel_png_chrm_matrix_t matrix;
    size_t i;
    size_t j;
    int status;

    status = sixel_png_chrm_build(&matrix, png_arith_grids[0], 2, 0);
    if (status == SIXEL_PNG_CHRM_UNSUPPORTED) {
        return 77;
    }
    if (status != 1 || matrix.identity != 1) {
        return 1;
    }
    /* Verify the exact rational identity itself, so the optimization cannot
     * hide a different target definition. No output epsilon is involved. */
    for (i = 0; i < 9; ++i) {
        if (i % 4 == 0) {
            if (matrix.n[i].sign != matrix.d.sign ||
                memcmp(matrix.n[i].w, matrix.d.w, sizeof(matrix.d.w))) {
                return 1;
            }
        } else {
            if (matrix.n[i].sign != 0) {
                return 1;
            }
            for (j = 0; j < 16; ++j) {
                if (matrix.n[i].w[j] != 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int
png_arith_build_atomic(void)
{
    int64_t grid[8] = {31270,32900,20000,20000,21000,20001,22000,20002};
    sixel_png_chrm_matrix_t matrix;
    unsigned char before[sizeof(matrix)];
    int mode;

    for (mode = 0; mode < 3; ++mode) {
        memset(&matrix, 0xa5, sizeof(matrix));
        memcpy(before, &matrix, sizeof(before));
        if (sixel_png_chrm_build(&matrix, grid, mode, 1) != 0 ||
            memcmp(before, &matrix, sizeof(before))) {
            return 1;
        }
    }
    return 0;
}

int
png_arith_pixel_atomic(int alias)
{
    static uint32_t const invalid[] = {
        0x7fc00000u, 0x7f800000u, 0xff800000u, 0xbf000000u, 0x3f800001u
    };
    sixel_png_chrm_matrix_t matrix;
    sixel_png_chrm_stats_t stats;
    sixel_png_chrm_stats_t before_stats;
    float input[6];
    float output[6];
    float *destination;
    unsigned char before[sizeof(output)];
    size_t i;
    int mode;
    int status;

    for (mode = 0; mode < 3; ++mode) {
        status = sixel_png_chrm_build(&matrix, png_arith_grids[5], mode, 1);
        if (status == SIXEL_PNG_CHRM_UNSUPPORTED) {
            return 77;
        }
        if (status != 1) {
            return 1;
        }
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            memset(input, 0, sizeof(input));
            memcpy(input + 4, invalid + i, sizeof(input[0]));
            memset(output, 0xa5, sizeof(output));
            destination = alias ? input : output;
            memcpy(before, destination, sizeof(before));
            memset(&stats, 0xa5, sizeof(stats));
            memcpy(&before_stats, &stats, sizeof(stats));
            if (sixel_png_chrm_apply(&matrix, input, destination, 2, &stats)
                    != SIXEL_PNG_CHRM_INVALID_PIXELS ||
                memcmp(before, destination, sizeof(before)) ||
                memcmp(&before_stats, &stats, sizeof(stats))) {
                return 1;
            }
        }
    }
    return 0;
}

#if HAVE_LIBPNG
typedef struct {
    int sample;
    int exact;
    int calls;
} png_arith_capture_t;

static SIXELSTATUS
png_arith_capture(sixel_frame_t *frame, void *context)
{
    png_arith_capture_t *probe;
    uint32_t actual[24];
    float expected;
    float value;
    int i;

    probe = (png_arith_capture_t *)context;
    ++probe->calls;
    if (frame->width != 8 || frame->height != 1 ||
        frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        return SIXEL_BAD_INPUT;
    }
    memcpy(actual, sixel_frame_get_pixels(frame), sizeof(actual));
    for (i = 0; i < 24; ++i) {
        memcpy(&value, actual + i, sizeof(value));
        memcpy(&expected, &png_arith_loader_expected[probe->sample][i],
               sizeof(expected));
        if ((probe->exact && actual[i] !=
                png_arith_loader_expected[probe->sample][i]) ||
            (!probe->exact && (!isfinite(value) ||
                fabs((double)value - expected) > 2e-7))) {
            return SIXEL_BAD_INPUT;
        }
    }
    return SIXEL_OK;
}
#endif

int
png_arith_load(int sample, char const *order, int exact)
{
#if HAVE_LIBPNG
    sixel_loader_t *loader;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    png_arith_capture_t probe;
    SIXELSTATUS status;
    int result;
    int old_policy;

    if (sample < 0 || sample >= 8) {
        return 1;
    }
    old_policy = loader_transparent_policy();
    sixel_helper_set_loader_transparent_policy(SIXEL_ALPHA_POLICY_COMPOSITE);
    loader = NULL;
    allocator = NULL;
    chunk = NULL;
    probe.sample = sample;
    probe.exact = exact;
    probe.calls = 0;
    result = 1;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 order);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_chunk_create_from_memory(&chunk,
        png_arith_png[sample].bytes, png_arith_png[sample].size, NULL,
        allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &probe);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_load_chunk(loader, chunk, png_arith_capture);
    result = status != SIXEL_OK || probe.calls != 1;
end:
    if (result != 0) {
        fprintf(stderr, "PNG arithmetic load: status=%x, callbacks=%d: %s\n",
                status, probe.calls, sixel_helper_get_additional_message());
    }
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    if (loader != NULL) {
        sixel_loader_unref(loader);
    }
    sixel_allocator_unref(allocator);
    sixel_helper_set_loader_transparent_policy(old_policy);
    return result;
#else
    (void)sample;
    (void)order;
    (void)exact;
    return 77;
#endif
}
