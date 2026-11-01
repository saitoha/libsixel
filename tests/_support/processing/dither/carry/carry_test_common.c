/* SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/dither-carry.h"
#include "src/dither.h"
#include "src/lookup-policy.h"
#include "carry_test_common.h"

static int const carry_test_methods[] = {
    SIXEL_DIFFUSE_FS, SIXEL_DIFFUSE_ATKINSON, SIXEL_DIFFUSE_JAJUNI,
    SIXEL_DIFFUSE_STUCKI, SIXEL_DIFFUSE_BURKES, SIXEL_DIFFUSE_SIERRA1,
    SIXEL_DIFFUSE_SIERRA2, SIXEL_DIFFUSE_SIERRA3
};

int
carry_test_numeric(int scenario)
{
    sixel_dither_carry_kernel_t const *kernel;
    unsigned char source[3] = {100, 100, 100};
    unsigned char pixel[3];
    int32_t carry[3] = {300, 399, 0};
    int32_t candidate[3];
    int32_t expected[3] = {100, 100, 100};
    SIXELSTATUS status;
    int policy;
    int denominator;
    double threshold;
    double deadzone;
    int i;
    int j;
    int sum;
    int64_t energy;

    if (scenario == 0 || scenario == 8) {
        for (i = 0; i < 8; ++i) {
            kernel = sixel_dither_carry_kernel(carry_test_methods[i]);
            sum = 0;
            energy = 0;
            for (j = 0; j < kernel->count; ++j) {
                sum += kernel->taps[j].weight;
                energy += -137 * kernel->taps[j].weight;
                if (kernel->taps[j].dy >= kernel->rows) return 1;
            }
            /* Atkinson intentionally sends six eighths; all other stencils
             * conserve signed numerator energy before boundary clipping.
             */
            if (sum != (i == 1 ? 6 : kernel->denominator)) return 1;
            if (energy != -137 * sum) return 1;
            if (i == 6 && kernel->rows != 2) return 1;
        }
        return 0;
    }
    policy = SIXEL_DITHER_CLAMP_SOFT;
    threshold = 1.01;
    denominator = 100;
    deadzone = 5.0;
    if (scenario == 2) {
        carry[1] = 400;
        expected[0] = 103;
        expected[1] = 104;
    } else if (scenario == 3) {
        deadzone = 0.0;
        expected[0] = 103;
        expected[1] = 104;
    } else if (scenario >= 4 && scenario <= 7) {
        source[0] = 250;
        source[1] = source[2] = 120;
        carry[0] = 40;
        carry[1] = 20;
        carry[2] = -20;
        denominator = 1;
        threshold = 1.0 + 15.0 / 255.0;
        expected[0] = 270;
        expected[1] = 130;
        expected[2] = 110;
        if (scenario == 5) {
            source[0] = 100;
            expected[0] = 140;
            expected[1] = 140;
            expected[2] = 100;
        } else if (scenario == 6) {
            policy = SIXEL_DITHER_CLAMP_HARD;
            expected[0] = 255;
            expected[1] = 140;
            expected[2] = 100;
        } else if (scenario == 7) {
            policy = SIXEL_DITHER_CLAMP_NONE;
            expected[0] = 290;
            expected[1] = 140;
            expected[2] = 100;
        }
    } else if (scenario == 9) {
        policy = SIXEL_DITHER_CLAMP_NONE;
        denominator = 2;
        deadzone = 0.0;
        memset(source, 0, sizeof(source));
        for (i = -7; i <= 7; ++i) {
            carry[0] = i;
            status = sixel_dither_carry_candidate(source, carry, 1, 2,
                policy, threshold, deadzone, candidate, pixel);
            if (SIXEL_FAILED(status)) return 1;
            carry[0] = -i;
            status = sixel_dither_carry_candidate(source, carry, 1, 2,
                policy, threshold, deadzone, expected, pixel);
            if (SIXEL_FAILED(status) || candidate[0] != -expected[0]) return 1;
        }
        return 0;
    }
    status = sixel_dither_carry_candidate(source, carry, 3, denominator,
        policy, threshold, deadzone, candidate, pixel);
    if (SIXEL_FAILED(status)) return 1;
    for (i = 0; i < 3; ++i) {
        if (candidate[i] != expected[i]) {
            fprintf(stderr, "channel %d: %d != %d\n",
                    i, (int)candidate[i], (int)expected[i]);
            return 1;
        }
        if (pixel[i] != (expected[i] < 0 ? 0 :
                        expected[i] > 255 ? 255 : expected[i])) return 1;
    }
    return 0;
}

static sixel_lookup_policy_result_t
carry_test_lookup(sixel_lookup_policy_interface_t const *self,
                  unsigned char const *pixel)
{
    (void)self;
    return (int)pixel[0] + pixel[1] + pixel[2] >= 383;
}

static int
carry_test_hint(sixel_lookup_policy_interface_t const *self,
                unsigned char const *pixel, int hint)
{
    (void)hint;
    return carry_test_lookup(self, pixel);
}

static sixel_lookup_policy_vtbl_t const carry_test_lookup_vtbl = {
    NULL, NULL, NULL, carry_test_lookup, carry_test_hint,
    sixel_lookup_policy_map_pixels_default, NULL,
    NULL
};

int
carry_test_bias(int method)
{
    static int const grays[] = {64,80,96,112,127,128,143,159,175,191};
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_policy_effective_t request;
    sixel_lookup_policy_interface_t lookup;
    unsigned char palette[6] = {51,51,51,204,204,204};
    unsigned char *pixels;
    sixel_index_t *result;
    int gray;
    int tone;
    double biases[10];
    int i;
    int pass;
    double sum;
    double bias;
    SIXELSTATUS status;

    allocator = NULL;
    dither = NULL;
    pixels = NULL;
    result = NULL;
    pass = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator,
            NULL, NULL, NULL, NULL))) goto end;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, allocator))) goto end;
    pixels = malloc(512U * 512U * 3U);
    result = malloc(512U * 512U);
    if (pixels == NULL || result == NULL) goto end;
    memset(&request, 0, sizeof(request));
    lookup.vtbl = &carry_test_lookup_vtbl;
    request.dither = dither;
    request.lookup_policy = &lookup;
    request.width = request.height = 512;
    request.depth = 3;
    request.data = pixels;
    request.result = result;
    request.palette = palette;
    request.method_for_scan = SIXEL_SCAN_SERPENTINE;
    pass = 1;
    for (tone = 0; tone < 10; ++tone) {
        gray = grays[tone];
        memset(pixels, gray, 512U * 512U * 3U);
        status = sixel_dither_carry_apply(&request, carry_test_methods[method]);
        if (SIXEL_FAILED(status)) { pass = 0; break; }
        sum = 0.0;
        for (i = 0; i < 512 * 512; ++i) {
            sum += palette[3 * result[i]];
        }
        bias = sum / (512.0 * 512.0) - gray;
        fprintf(stderr, "kernel=%d gray=%d bias=%.6f\n", method, gray, bias);
        /* Full-image means include discarded boundary energy. Unit-sum
         * kernels measured below 0.21 levels on this fixture; 0.5 leaves
         * finite-image margin without accepting the former rounding bias.
         * Atkinson intentionally discards 2/8: absolute tone bias is not a
         * rounding contract, so paired complementary tones below test its
         * sign symmetry instead of pretending it conserves energy.
         */
        biases[tone] = bias;
        if (method != 1 && fabs(bias) > 0.5) pass = 0;
        for (i = 0; i < 512 * 512 * 3; ++i) {
            if (pixels[i] != gray) { pass = 0; break; }
        }
    }
    if (pass && method == 1) {
        for (tone = 0; tone < 5; ++tone) {
            if (fabs(biases[tone] + biases[9 - tone]) > 0.5) pass = 0;
        }
    }
end:
    free(result);
    free(pixels);
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return pass ? 0 : 1;
}

/* Capture lookup candidates rather than observing obsolete input mutation. */
typedef struct carry_capture_lookup {
    sixel_lookup_policy_interface_t base;
    unsigned char last[3];
    unsigned char seen[75];
    int calls;
    int fixed;
} carry_capture_lookup_t;

static sixel_lookup_policy_result_t
carry_capture_pixel(sixel_lookup_policy_interface_t const *base,
                     unsigned char const *pixel)
{
    carry_capture_lookup_t *self;

    self = (carry_capture_lookup_t *)(void *)base;
    memcpy(self->last, pixel, 3);
    if (self->calls < 25) memcpy(self->seen + self->calls * 3, pixel, 3);
    ++self->calls;
    if (self->fixed) return 0;
    return pixel[0] >= 128;
}

static int
carry_capture_hint(sixel_lookup_policy_interface_t const *base,
                    unsigned char const *pixel, int hint)
{
    (void)hint;
    return carry_capture_pixel(base, pixel);
}

static sixel_lookup_policy_vtbl_t const carry_capture_vtbl = {
    NULL, NULL, NULL, carry_capture_pixel, carry_capture_hint,
    sixel_lookup_policy_map_pixels_default, NULL,
    NULL
};

int
carry_test_keep(int method, int use_perturb, int early)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_policy_effective_t request;
    carry_capture_lookup_t lookup;
    sixel_dither_perturb_t perturb;
    unsigned char palette[6] = {0,0,0,255,255,255};
    unsigned char pixels[6] = {132,132,132,130,130,130};
    unsigned char original[6];
    unsigned char retained[6] = {250,250,250,0,0,0};
    unsigned char valid[2] = {1,0};
    sixel_index_t result[2];
    int weights[4];
    int expected;
    int n;
    int ok;
    SIXELSTATUS status;

    allocator = NULL;
    dither = NULL;
    ok = 0;
    memcpy(original, pixels, sizeof(pixels));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator,
            NULL, NULL, NULL, NULL))) goto end;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, allocator))) goto end;
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_transparent(dither, 0);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    dither->diffusion_deadzone = 0.0;
    dither->clamp_policy = SIXEL_DITHER_CLAMP_HARD;
    if (early) memset(retained, 255, 3);
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither, retained,
        sizeof(retained), valid, sizeof(valid), 2, 1, 0, 0, 2, 1, 0, 1,
        early ? 255u : 0u, SIXEL_6DELTA_ERROR_DIFFUSE);
    memset(&request, 0, sizeof(request));
    memset(&lookup, 0, sizeof(lookup));
    lookup.base.vtbl = &carry_capture_vtbl;
    request.dither = dither;
    request.lookup_policy = &lookup.base;
    request.data = pixels;
    request.result = result;
    request.palette = palette;
    request.width = 2;
    request.height = 1;
    request.depth = 3;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    /* Independent first-neighbor values for source 132, retained 250,
     * next source 130, and each canonical first tap. The palette residual
     * instead of the retained residual yields a different rounded value.
     */
    switch (method) {
    case SIXEL_DIFFUSE_JAJUNI: expected = 113; break;
    case SIXEL_DIFFUSE_STUCKI: expected = 108; break;
    case SIXEL_DIFFUSE_BURKES: expected = 100; break;
    case SIXEL_DIFFUSE_SIERRA1: expected = 71; break;
    case SIXEL_DIFFUSE_SIERRA2: expected = 100; break;
    case SIXEL_DIFFUSE_SIERRA3: expected = 112; break;
    default: expected = 78; break;
    }
    if (use_perturb) {
        dither->diffusion_perturb = 0.5f;
        sixel_dither_perturb_init(&perturb, 0.5f, 0);
        sixel_dither_perturb_weights(&perturb, 0, 0, weights);
        /* These positive candidates do not land on a half tie. */
        expected = (int)floor(130.0 +
            (132 - (early ? 255 : 250)) * weights[0] / 4096.0 + 0.5);
    }
    status = sixel_dither_carry_apply(&request, method);
    ok = SIXEL_SUCCEEDED(status) && result[0] == 0
        && memcmp(pixels, original, sizeof(pixels)) == 0;
    for (n = 0; n < 3; ++n) {
        if (lookup.last[n] != expected) {
            fprintf(stderr, "retained candidate %u != %d\n",
                    (unsigned int)lookup.last[n], expected);
            ok = 0;
        }
    }
end:
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}

/* A preceding quantized pixel puts nonzero carry into the kept pixel. This
 * distinguishes candidate-minus-retained feedback from source-minus-retained.
 */
int
carry_test_keep_incoming(int early)
{
    sixel_dither_t *dither;
    sixel_dither_policy_effective_t request;
    carry_capture_lookup_t lookup;
    unsigned char palette[6] = {0,0,0,255,255,255};
    unsigned char pixels[9] = {64,64,64,132,132,132,130,130,130};
    unsigned char original[9];
    unsigned char retained[9] = {0,0,0,250,250,250,0,0,0};
    unsigned char valid[3] = {0,1,0};
    sixel_index_t result[3];
    SIXELSTATUS status;
    int expected;
    int ok;
    int n;

    dither = NULL;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, NULL))) return 1;
    memcpy(original, pixels, sizeof(pixels));
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_transparent(dither, 0);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
    dither->diffusion_deadzone = 0.0;
    dither->clamp_policy = SIXEL_DITHER_CLAMP_HARD;
    if (early) memset(retained + 3, 255, 3);
    sixel_dither_set_pipeline_accumulation_buffer_hint(dither, retained,
        sizeof(retained), valid, sizeof(valid), 3, 1, 0, 0, 3, 1, 0, 1,
        early ? 255u : 0u, SIXEL_6DELTA_ERROR_DIFFUSE);
    memset(&request, 0, sizeof(request));
    memset(&lookup, 0, sizeof(lookup));
    lookup.base.vtbl = &carry_capture_vtbl;
    request.dither = dither;
    request.lookup_policy = &lookup.base;
    request.data = pixels;
    request.result = result;
    request.palette = palette;
    request.width = 3;
    request.height = 1;
    request.depth = 3;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    /* First residual 64 adds 28 to the second source: candidate=160.
     * Its retained residual is -95 or -90, giving third candidate 88 or 91.
     */
    expected = early ? 88 : 91;
    status = sixel_dither_carry_apply(&request, SIXEL_DIFFUSE_FS);
    ok = SIXEL_SUCCEEDED(status) && result[1] == 0
        && memcmp(pixels, original, sizeof(pixels)) == 0;
    for (n = 0; n < 3; ++n) {
        if (lookup.last[n] != expected) ok = 0;
    }
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}

int
carry_test_pipeline(int scenario)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_policy_effective_t request;
    sixel_dither_carry_kernel_t const *kernel;
    carry_capture_lookup_t lookup;
    unsigned char pixels[75];
    unsigned char palette[3] = {51,51,51};
    sixel_index_t result[25];
    int64_t reference[25];
    int64_t total;
    int64_t escaped;
    int64_t distributed;
    int64_t residual;
    int64_t numerator;
    int rounded;
    int remainder;
    int m;
    int pos;
    int x;
    int y;
    int tap;
    int nx;
    int ny;
    int ok;

    (void)scenario;
    allocator = NULL;
    dither = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator,
            NULL, NULL, NULL, NULL))) goto end;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 1, allocator))) goto end;
    dither->diffusion_deadzone = 0.0;
    memset(&request, 0, sizeof(request));
    request.dither = dither;
    request.data = pixels;
    request.result = result;
    request.palette = palette;
    request.width = request.height = 5;
    request.depth = 3;
    request.lookup_policy = &lookup.base;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    for (m = 0; m < 8; ++m) {
        memset(&lookup, 0, sizeof(lookup));
        memset(pixels, 50, sizeof(pixels));
        memset(reference, 0, sizeof(reference));
        lookup.base.vtbl = &carry_capture_vtbl;
        lookup.fixed = 1;
        kernel = sixel_dither_carry_kernel(carry_test_methods[m]);
        if (SIXEL_FAILED(sixel_dither_carry_apply(&request,
                carry_test_methods[m]))) goto end;
        total = escaped = distributed = 0;
        for (pos = 0; pos < 25; ++pos) {
            /* Independent full-frame int64 oracle. Reverse tap insertion
             * verifies the ring implementation against a different ordering.
             * Coordinates stay inside the cube, so soft scaling is inactive.
             */
            numerator = 50 * kernel->denominator + reference[pos];
            rounded = (int)(numerator / kernel->denominator);
            remainder = (int)(numerator % kernel->denominator);
            if (2 * remainder > kernel->denominator ||
                    (2 * remainder == kernel->denominator && rounded % 2)) {
                ++rounded;
            }
            if (lookup.seen[pos * 3] != rounded || pixels[pos * 3] != 50) {
                fprintf(stderr, "conservation kernel=%d position=%d\n", m, pos);
                goto end;
            }
            residual = rounded - 51;
            total += residual * (m == 1 ? 6 : kernel->denominator);
            x = pos % 5;
            y = pos / 5;
            for (tap = kernel->count - 1; tap >= 0; --tap) {
                nx = x + kernel->taps[tap].dx;
                ny = y + kernel->taps[tap].dy;
                numerator = residual * kernel->taps[tap].weight;
                if (nx < 0 || nx >= 5 || ny >= 5) {
                    escaped += numerator;
                } else {
                    reference[ny * 5 + nx] += numerator;
                    distributed += numerator;
                }
            }
        }
        if (total != escaped + distributed || lookup.calls != 25) goto end;
    }
    ok = 1;
end:
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
