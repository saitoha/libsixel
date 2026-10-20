/* SPDX-License-Identifier: MIT */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "src/dither-carry.h"
#include "carry_speed_common.h"
#include "src/dither.h"
#include "src/lookup-policy.h"
#include "src/dither-common-pipeline.h"

/* Frozen double candidate from 1516ae5db. This oracle deliberately uses no
 * production rounding or deadzone helpers: these tests enforce compatibility
 * with that arithmetic, rather than comparing two copies of the fast path.
 */
static SIXELSTATUS
carry_speed_reference(unsigned char const *source,
                            int32_t const *carry, int depth, int denominator,
                            int policy, double threshold, double deadzone,
                            int32_t *candidate, unsigned char *lookup)
{
    double norm;
    double radius;
    double scale;
    double margin;
    double delta;
    double value;
    double lower;
    double fraction;
    int n;
    int32_t rounded;

    norm = 0.0;
    for (n = 0; n < depth; ++n) {
        norm += (double)carry[n] * (double)carry[n];
    }
    /* Comparing the squared norm needs no square root. Double avoids an
     * int64 sum overflow for unbounded NONE feedback. A radius beyond the
     * maximum possible int32 vector discards every correction directly.
     */
    radius = (deadzone > 1.0e10 ? 1.0e10 : deadzone) * denominator;
    scale = norm < radius * radius ? 0.0 : 1.0;
    margin = (threshold - 1.0) * 255.0;
    if (scale != 0.0 && policy != SIXEL_DITHER_CLAMP_NONE
            && policy != SIXEL_DITHER_CLAMP_HARD) {
        /* Compare in carry units; divide only if a boundary is crossed.
         * One common scale preserves the RGB correction direction.
         */
        for (n = 0; n < depth; ++n) {
            delta = carry[n];
            if (delta * scale < (-margin - source[n]) * denominator) {
                scale = (-margin - source[n]) * denominator / delta;
            } else if (delta * scale >
                       (255.0 + margin - source[n]) * denominator) {
                scale = (255.0 + margin - source[n]) * denominator / delta;
            }
        }
    }
    for (n = 0; n < depth; ++n) {
        value = source[n] + (double)carry[n] * scale / denominator;
        if (policy == SIXEL_DITHER_CLAMP_HARD) {
            if (value < 0.0) value = 0.0;
            if (value > 255.0) value = 255.0;
        }
        /* Nearest integer, exact half ties to even, independent of the
         * floating-point rounding mode. This is symmetric for both signs;
         * unlike adding 0.5 then truncating it does not favor positive error.
         */
        lower = floor(value);
        if (lower < INT32_MIN || lower > INT32_MAX - 1) {
            return SIXEL_BAD_INPUT;
        }
        rounded = (int32_t)lower;
        fraction = value - lower;
        if (fraction > 0.5 || (fraction == 0.5 && rounded % 2 != 0)) {
            ++rounded;
        }
        candidate[n] = rounded;
        lookup[n] = (unsigned char)(rounded < 0 ? 0 :
                                   rounded > 255 ? 255 : rounded);
    }
    return SIXEL_OK;
}

static int
carry_speed_compare(unsigned char const *source, int32_t const *carry,
                    int denominator, int policy, double threshold,
                    double deadzone)
{
    int32_t actual[3];
    int32_t expected[3];
    unsigned char pixel[3];
    unsigned char reference[3];
    SIXELSTATUS a;
    SIXELSTATUS b;

    a = sixel_dither_carry_candidate(source, carry, 3, denominator, policy,
                                     threshold, deadzone, actual, pixel);
    b = carry_speed_reference(source, carry, 3, denominator, policy,
                              threshold, deadzone, expected, reference);
    if (a != b || (SIXEL_SUCCEEDED(a) &&
            (memcmp(actual, expected, sizeof(actual)) != 0 ||
             memcmp(pixel, reference, sizeof(pixel)) != 0))) {
        fprintf(stderr, "den=%d policy=%d threshold=%.17g deadzone=%.17g "
                "source=%u,%u,%u carry=%d,%d,%d\n", denominator, policy,
                threshold, deadzone, source[0], source[1], source[2],
                (int)carry[0], (int)carry[1], (int)carry[2]);
        return 1;
    }
    return 0;
}

int
carry_speed_numeric(int boundary)
{
    static int const denominators[] = {4,8,16,32,42,48,4096};
    static int const policies[] = {SIXEL_DITHER_CLAMP_SOFT,
        SIXEL_DITHER_CLAMP_HARD, SIXEL_DITHER_CLAMP_NONE};
    static double const zones[] = {0.0,3.25,5.0,1.0e300};
    unsigned char source[3];
    int32_t carry[3];
    uint32_t random;
    double norm;
    double radius;
    double threshold;
    double margin;
    int d;
    int i;
    int j;
    int k;
    int den;
    int sign;
    int n;

    random = 123456789u;
    for (d = 0; d < 7; ++d) {
        den = denominators[d];
        if (!boundary) {
            /* Every half-integer and both adjacent numerator values over
             * the full bounded candidate interval, for all denominators.
             * Source offsets exercise cancellation and both parity signs.
             */
            for (i = -4080; i <= 4080; ++i) {
                for (j = -1; j <= 1; ++j) {
                    for (n = 0; n < 3; ++n) {
                        source[n] = (unsigned char)(i + n * 127);
                        carry[n] = (i - source[n]) * den + den / 2 + j;
                    }
                    if (carry_speed_compare(source, carry, den,
                            SIXEL_DITHER_CLAMP_SOFT, 16.0, 0.0)) return 1;
                }
            }
        } else {
            /* Every byte source and both integer neighbors of each soft
             * bound, including limits adjacent to a nonbinary threshold.
             */
            for (i = 0; i < 256; ++i) {
                for (k = 0; k < 4; ++k) {
                    threshold = k == 0 ? 1.0 : k == 1 ? 1.01 :
                        k == 2 ? 16.0 : nextafter(1.01, 0.0);
                    margin = (threshold - 1.0) * 255.0;
                    for (sign = -1; sign <= 1; sign += 2) {
                        for (j = -1; j <= 1; ++j) {
                            for (n = 0; n < 3; ++n) {
                                source[n] = (unsigned char)(i + n * 43);
                                carry[n] = (int32_t)(sign < 0 ?
                                    ceil((-margin - source[n]) * den) :
                                    floor((255 + margin - source[n]) * den));
                                carry[n] += j;
                            }
                            if (carry_speed_compare(source, carry, den,
                                    SIXEL_DITHER_CLAMP_SOFT, threshold, 0.0)) {
                                return 1;
                            }
                        }
                    }
                }
            }
        }
        /* Deterministic samples span the complete bounded carry range and
         * the full int32 NONE domain, with spherical deadzone boundaries
         * approached from both adjacent representable double thresholds.
         */
        for (i = 0; i < 10000; ++i) {
            for (n = 0; n < 3; ++n) {
                random = random * 1664525u + 1013904223u;
                source[n] = (unsigned char)(random >> 24);
                carry[n] = (int32_t)(random % 58490881u) - 29245440;
            }
            threshold = i & 1 ? 1.01 : 16.0;
            if (boundary) {
                norm = (double)carry[0] * carry[0] +
                    (double)carry[1] * carry[1] +
                    (double)carry[2] * carry[2];
                radius = sqrt(norm) / den;
                for (j = -1; j <= 1; ++j) {
                    if (carry_speed_compare(source, carry, den,
                            SIXEL_DITHER_CLAMP_SOFT, threshold,
                            j < 0 ? nextafter(radius, 0.0) :
                            j > 0 ? nextafter(radius, INFINITY) : radius)) {
                        return 1;
                    }
                }
            } else {
                for (j = 0; j < 3; ++j) {
                    if (carry_speed_compare(source, carry, den, policies[j],
                            threshold, zones[i % 4])) return 1;
                }
                carry[0] = INT32_MIN + i;
                carry[1] = INT32_MAX - i;
                if (carry_speed_compare(source, carry, den,
                        SIXEL_DITHER_CLAMP_NONE, threshold, zones[i % 4])) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* Frozen generic scatter/scan loop, including masks, keep and overflow. */
static SIXELSTATUS
carry_speed_apply_reference(sixel_dither_policy_apply_request_t const *request,
                        int method)
{
    sixel_dither_carry_kernel_t const *kernel;
    sixel_dither_t *dither;
    sixel_lookup_policy_interface_t const *lookup;
    sixel_dither_perturb_t perturb;
    int32_t *rows;
    int32_t *current;
    int32_t *destination;
    int32_t candidate[SIXEL_MAX_CHANNELS];
    unsigned char pixel[SIXEL_MAX_CHANNELS];
    unsigned char const *target;
    unsigned char const *retained;
    size_t stride;
    size_t position;
    size_t absolute_index;
    int width;
    int depth;
    int y;
    int x;
    int absolute_y;
    int direction;
    int stop;
    int n;
    int tap;
    int nx;
    int ny;
    int hint;
    int index;
    int keycolor;
    int keep;
    int record;
    int denominator;
    int weights[4];
    int weight;
    int64_t sum;
    SIXELSTATUS status;

    if (request == NULL || request->dither == NULL
            || request->data == NULL || request->palette == NULL
            || request->result == NULL || request->width <= 0
            || request->height <= 0 || request->depth <= 0
            || request->depth > SIXEL_MAX_CHANNELS) {
        return SIXEL_BAD_ARGUMENT;
    }
    kernel = sixel_dither_carry_kernel(method);
    lookup = request->lookup_policy;
    if (kernel == NULL || lookup == NULL || lookup->vtbl == NULL
            || lookup->vtbl->map_pixel_hint == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    dither = request->dither;
    width = request->width;
    depth = request->depth;
    stride = (size_t)width * (size_t)depth;
    if (stride / (size_t)depth != (size_t)width
            || stride > SIZE_MAX / sizeof(*rows) / kernel->rows) {
        return SIXEL_BAD_ALLOCATION;
    }
    /* Allocate once per band. No state is shared across overlapping bands;
     * the caller supplies warm-up rows and an absolute output boundary.
     * Allocation failure is an error, never an output-changing fallback.
     */
    rows = sixel_allocator_calloc(dither->allocator,
                                  stride * kernel->rows, sizeof(*rows));
    if (rows == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    sixel_dither_perturb_init(&perturb, dither->diffusion_perturb,
                              dither->diffusion_perturb_seed);
    perturb.enabled = perturb.enabled && method == SIXEL_DIFFUSE_FS;
    denominator = perturb.enabled ? 16 << 8 : kernel->denominator;
    hint = -1;
    status = SIXEL_OK;
    for (y = 0; y < request->height; ++y) {
        absolute_y = request->band_origin + y;
        direction = request->method_for_scan == SIXEL_SCAN_SERPENTINE
            && (absolute_y & 1) ? -1 : 1;
        x = direction > 0 ? 0 : width - 1;
        stop = direction > 0 ? width : -1;
        for (; x != stop; x += direction) {
            position = (size_t)y * width + x;
            absolute_index = absolute_y < 0 ? 0 :
                (size_t)absolute_y * width + x;
            record = absolute_y >= request->output_start;
            current = rows + (size_t)(y % kernel->rows) * stride
                + (size_t)x * depth;
            /* Transparent destinations absorb incoming correction. Taps may
             * jump over a transparent pixel as in the existing stencil;
             * the masked pixel never looks up or relays a residual.
             */
            if (absolute_y >= 0 && dither->pipeline_transparent_mask != NULL
                    && dither->pipeline_transparent_keycolor >= 0
                    && dither->pipeline_transparent_keycolor < SIXEL_PALETTE_MAX
                    && absolute_index < dither->pipeline_transparent_mask_size
                    && dither->pipeline_transparent_mask[absolute_index]) {
                if (record) {
                    request->result[position] = (sixel_index_t)
                        dither->pipeline_transparent_keycolor;
                }
                continue;
            }
            status = carry_speed_reference(
                request->data + position * depth, current, depth, denominator,
                dither->clamp_policy, dither->clamp_threshold,
                dither->diffusion_deadzone,
                candidate, pixel);
            if (SIXEL_FAILED(status)) goto end;
            retained = NULL;
            keycolor = -1;
            keep = 0;
            if (absolute_y >= 0 && depth >= 3) {
                keep = sixel_dither_pipeline_6delta_try_keep_rgb888(
                    dither, absolute_index, x, absolute_y, pixel, record,
                    &retained, &keycolor);
            }
            index = keycolor;
            if (!keep) {
                index = lookup->vtbl->map_pixel_hint(lookup, pixel, hint);
                if (index < 0 || index >= SIXEL_PALETTE_MAX) {
                    status = SIXEL_BAD_INPUT;
                    goto end;
                }
                hint = index;
                if (absolute_y >= 0 && depth >= 3) {
                    keep = sixel_dither_pipeline_6delta_try_keep_after_lookup(
                        dither, absolute_index, x, absolute_y, pixel,
                        request->palette + (size_t)index * depth, index,
                        record, &retained, &keycolor);
                }
            }
            if (record) {
                request->result[position] = (sixel_index_t)
                    (keep ? keycolor : index);
            }
            if (keep && sixel_dither_pipeline_6delta_error_mode(dither)
                    != SIXEL_6DELTA_ERROR_DIFFUSE) {
                continue;
            }
            target = keep ? retained : request->palette + (size_t)index * depth;
            if (perturb.enabled) {
                sixel_dither_perturb_weights(&perturb, x, absolute_y, weights);
            }
            for (tap = 0; tap < kernel->count; ++tap) {
                nx = x + direction * kernel->taps[tap].dx;
                ny = y + kernel->taps[tap].dy;
                if (nx < 0 || nx >= width || ny >= request->height) continue;
                destination = rows + (size_t)(ny % kernel->rows) * stride
                    + (size_t)nx * depth;
                weight = perturb.enabled ? weights[tap]
                    : kernel->taps[tap].weight;
                for (n = 0; n < depth; ++n) {
                    /* At default soft bounds |error| <= 258. Fixed kernels
                     * sum to at most 48; perturbed FS incoming weights sum
                     * to at most 28*256 even with independently varied taps.
                     * Thus |carry| <= 258*7168 = 1849344 < INT32_MAX.
                     * Even threshold=16 is safe (4080*7168 < INT32_MAX).
                     * NONE has no such bound: check widened arithmetic and
                     * return an error if feedback exceeds int32 capacity.
                     */
                    sum = destination[n] +
                        ((int64_t)candidate[n] - target[n]) * weight;
                    if (sum < INT32_MIN || sum > INT32_MAX) {
                        status = SIXEL_BAD_INPUT;
                        goto end;
                    }
                    destination[n] = (int32_t)sum;
                }
            }
        }
        memset(rows + (size_t)(y % kernel->rows) * stride, 0,
               stride * sizeof(*rows));
        if (absolute_y >= request->output_start) {
            sixel_dither_pipeline_row_notify(dither, absolute_y);
        }
    }
end:
    sixel_allocator_free(dither->allocator, rows);
    return status;
}

typedef struct carry_speed_capture {
    sixel_lookup_policy_interface_t base;
    unsigned char seen[9 * 6 * 3];
    int calls;
} carry_speed_capture_t;

static int
carry_speed_lookup(sixel_lookup_policy_interface_t const *base,
                    unsigned char const *pixel, int hint)
{
    carry_speed_capture_t *capture;

    (void)hint;
    capture = (carry_speed_capture_t *)(void *)base;
    memcpy(capture->seen + capture->calls * 3, pixel, 3);
    ++capture->calls;
    return (pixel[0] >= 128 ? 4 : 0) + (pixel[1] >= 128 ? 2 : 0)
        + (pixel[2] >= 128 ? 1 : 0);
}

static sixel_lookup_policy_vtbl_t const carry_speed_vtbl = {
    NULL, NULL, NULL, NULL, carry_speed_lookup,
    sixel_lookup_policy_map_pixels_default, NULL
};

int
carry_speed_stencil(void)
{
    static int const methods[] = {SIXEL_DIFFUSE_FS, SIXEL_DIFFUSE_ATKINSON,
        SIXEL_DIFFUSE_JAJUNI, SIXEL_DIFFUSE_STUCKI, SIXEL_DIFFUSE_BURKES,
        SIXEL_DIFFUSE_SIERRA1, SIXEL_DIFFUSE_SIERRA2, SIXEL_DIFFUSE_SIERRA3};
    sixel_dither_t *dither[2] = {NULL, NULL};
    sixel_dither_policy_apply_request_t request;
    carry_speed_capture_t capture[2];
    unsigned char source[9 * 6 * 3];
    unsigned char retained[9 * 8 * 3];
    unsigned char mask[9 * 8];
    unsigned char valid[9 * 8];
    unsigned char palette[8 * 3];
    sixel_index_t result[2][9 * 6];
    SIXELSTATUS status[2];
    int w;
    int h;
    int m;
    int mode;
    int origin;
    int n;
    int side;
    int ok;
    uint32_t random;

    ok = 0;
    for (side = 0; side < 2; ++side) {
        if (SIXEL_FAILED(sixel_dither_new(&dither[side], 8, NULL))) goto end;
        sixel_dither_set_pixelformat(dither[side], SIXEL_PIXELFORMAT_RGB888);
        sixel_dither_set_transparent(dither[side], 0);
    }
    for (n = 0; n < 8; ++n) {
        palette[n * 3] = n & 4 ? 255 : 0;
        palette[n * 3 + 1] = n & 2 ? 255 : 0;
        palette[n * 3 + 2] = n & 1 ? 255 : 0;
    }
    random = 314159265u;
    for (n = 0; n < (int)sizeof(source); ++n) {
        random = random * 1664525u + 1013904223u;
        source[n] = (unsigned char)(random >> 24);
    }
    for (n = 0; n < (int)sizeof(mask); ++n) {
        mask[n] = n % 5 == 0;
        valid[n] = n % 3 != 0;
        memcpy(retained + n * 3, palette + (n % 8) * 3, 3);
    }
    memset(&request, 0, sizeof(request));
    request.palette = palette;
    request.data = source;
    request.depth = 3;
    /* All edge widths, bottom-row counts, scan parities, warm-up origins,
     * mask presence, early/late keep, skip/diffuse and perturbed FS. Capture
     * candidates as well as indices so coarse palette bins cannot hide drift.
     */
    for (w = 1; w <= 9; ++w) {
        for (h = 1; h <= 6; ++h) {
            for (m = 0; m < 8; ++m) {
                for (mode = 0; mode < 16; ++mode) {
                    for (origin = -1; origin <= 1; ++origin) {
                        request.width = w;
                        request.height = h;
                        request.band_origin = origin;
                        request.output_start = mode & 4 ? origin + 2 : 0;
                        request.method_for_scan = mode & 1 ?
                            SIXEL_SCAN_SERPENTINE : SIXEL_SCAN_RASTER;
                        memset(result, 0xa5, sizeof(result));
                        memset(capture, 0, sizeof(capture));
                        for (side = 0; side < 2; ++side) {
                            capture[side].base.vtbl = &carry_speed_vtbl;
                            request.dither = dither[side];
                            request.lookup_policy = &capture[side].base;
                            request.result = result[side];
                            dither[side]->clamp_policy = mode & 2 ?
                                SIXEL_DITHER_CLAMP_HARD :
                                SIXEL_DITHER_CLAMP_SOFT;
                            dither[side]->diffusion_deadzone =
                                mode & 4 ? 0.0 : 3.25;
                            dither[side]->diffusion_perturb =
                                mode & 8 ? 0.5f : 0.0f;
                            dither[side]->pipeline_transparent_mask =
                                mode & 2 ? mask : NULL;
                            dither[side]->pipeline_transparent_mask_size =
                                (size_t)w * h;
                            dither[side]->pipeline_transparent_keycolor = 0;
                            sixel_dither_set_pipeline_accumulation_buffer_hint(
                                dither[side], retained, sizeof(retained),
                                valid, sizeof(valid), w, h + 2, 0, 0, w, h,
                                0, 1, mode & 4 ? 255u : 8u,
                                mode & 1 ? SIXEL_6DELTA_ERROR_SKIP :
                                           SIXEL_6DELTA_ERROR_DIFFUSE);
                            dither[side]->pipeline_6delta_enabled =
                                !!(mode & 8);
                            status[side] = side == 0 ?
                                carry_speed_apply_reference(&request,
                                                             methods[m]) :
                                sixel_dither_carry_apply(&request, methods[m]);
                        }
                        if (SIXEL_FAILED(status[0]) || status[0] != status[1] ||
                                memcmp(result[0], result[1],
                                       sizeof(result[0])) != 0 ||
                                capture[0].calls != capture[1].calls ||
                                memcmp(capture[0].seen, capture[1].seen,
                                       sizeof(capture[0].seen)) != 0) {
                            fprintf(stderr, "w=%d h=%d m=%d mode=%d y=%d\n",
                                    w, h, m, mode, origin);
                            goto end;
                        }
                    }
                }
            }
        }
    }
    ok = 1;
end:
    sixel_dither_unref(dither[0]);
    sixel_dither_unref(dither[1]);
    return ok ? 0 : 1;
}
