/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "src/gpu-palette.h"
#include "src/bluenoise_64x64.h"
#include "src/dither.h"
#include "src/lookup-policy.h"
#include "src/encoder-core-private.h"
#include "component_common.h"

/* Metal answers byte 5bit and 6bit queries with its legacy table executor
 * (kernel 8). A device without that executor runs the policy's injected
 * RGB8 program (kernel 9), whose table is private to the lookup plan. */
static int
rgb_cell_expected_kernel(void)
{
    sixel_gpu_device_interface_t *device;
    int kernel;

    device = NULL;
    kernel = 8;
    if (sixel_gpu_device_acquire(&device) == SIXEL_OK && device != NULL) {
        if (strcmp(device->vtbl->program_format(device),
                "metal.lookup-rgb8.v1") != 0) {
            kernel = 9;
        }
        device->vtbl->unref(device);
    }
    return kernel;
}

/* Vulkan exposes its owned byte allocation through the prepared plan.
 * Inspect that contract instead of a Metal executor's diagnostic hook. */
static int
rgb_cell_table_size(int bits)
{
    gpu_component_fixture_t fixture;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_lookup_program_view_t view;
    int ok;

    device = NULL;
    memset(&fixture, 0, sizeof(fixture));
    ok = 0;
    if (sixel_gpu_device_acquire(&device) != SIXEL_OK ||
            !gpu_component_open(&fixture, bits == 5 ? 10 : 11)) goto end;
    if (fixture.provider->vtbl->prepare(fixture.provider, &fixture.request,
            device, &fixture.other) != SIXEL_OK ||
            fixture.other->vtbl->describe(fixture.other, &view) != SIXEL_OK)
        goto end;
    ok = view.workspace_size == ((size_t)1U << (bits * 3));
end:
    gpu_component_close(&fixture);
    if (device != NULL) device->vtbl->unref(device);
    return ok ? 0 : 1;
}

/* Enumerate a channel's byte preimage rather than repeating the GPU's
 * endpoint formula. Doubled centers and integer distances are exact. */
static int
rgb_cell_reference(unsigned char const *palette, int colors,
    unsigned char const *query, int bits)
{
    int center[3], axis, value, bucket, key, low, high, step, maximum;
    int i, d, distance, best, minimum;

    step = 1 << (8 - bits);
    maximum = (1 << bits) - 1;
    for (axis = 0; axis < 3; ++axis) {
        bucket = (query[axis] + step / 2) / step;
        if (bucket > maximum) bucket = maximum;
        low = 256;
        high = -1;
        for (value = 0; value < 256; ++value) {
            key = (value + step / 2) / step;
            if (key > maximum) key = maximum;
            if (key == bucket) {
                if (value < low) low = value;
                high = value;
            }
        }
        center[axis] = low + high;
    }
    minimum = INT_MAX;
    best = 0;
    for (i = 0; i < colors; ++i) {
        distance = 0;
        for (axis = 0; axis < 3; ++axis) {
            d = center[axis] - 2 * palette[3 * i + axis];
            distance += d * d;
        }
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    return best;
}

/* Fixed strength/phase overrides isolate LUT selection from option defaults.
 * Positional formulas are shared contracts; cell selection above has an
 * independent discrete oracle. No diffusion error propagates between pixels. */
static void
rgb_cell_query(unsigned char *query, unsigned char const *source, int i,
    int method)
{
    int c, x, y, value;
    unsigned int phase;
    float noise, u, v;

    x = i % 16;
    y = i / 16;
    for (c = 0; c < 3; ++c) {
        noise = 0;
        if (method == SIXEL_DIFFUSE_BLUENOISE_DITHER) {
            u = (sixel_bn_mask(x, y) + 1.0f) * 0.5f;
            v = (sixel_bn_mask(x + 13, y + 29) + 1.0f) * 0.5f;
            noise = u + v - 1.0f;
        } else if (method == SIXEL_DIFFUSE_A_DITHER) {
            phase = (unsigned int)(x + c * 67 + y * 236) * 119U;
            noise = (float)(phase & 255U) / 128.0f - 1.0f;
        } else if (method == SIXEL_DIFFUSE_X_DITHER) {
            phase = (unsigned int)((x + c * 29) ^ (y * 149)) * 1234U;
            noise = (float)(phase & 511U) / 256.0f - 1.0f;
        }
        value = source[c] + (int)(noise * 32.0f);
        query[c] = (unsigned char)(value < 0 ? 0 : value > 255 ? 255 : value);
    }
}

static int
rgb_cell_distance(unsigned char const *a, unsigned char const *b)
{
    int i, d, sum;

    sum = 0;
    for (i = 0; i < 3; ++i) {
        d = a[i] - b[i];
        sum += d * d;
    }
    return sum;
}

/* Each wrapper fixes one bit width and one observation. The byte ramp
 * crosses every rounding boundary without exhaustive RGB-space traversal. */
int
test_rgb_cell_gpu(int bits, int observation)
{
    sixel_gpu_palette_request_t request;
    sixel_gpu_palette_test_hooks_t hooks;
    unsigned char palette[768], pixels[768], query[3], transparent[256];
    unsigned char retained[20 * 20 * 3], valid[20 * 20], mask[256];
    sixel_index_t output[256], first[256];
    unsigned char swap[3];
    int i, c, pos, best, delta, keep, within_threshold;
    int offset, want_mask, colors;
    SIXELSTATUS status;

    memset(&request, 0, sizeof(request));
    memset(&hooks, 0, sizeof(hooks));
    memset(mask, 0, sizeof(mask));
    memset(transparent, 0, sizeof(transparent));
    for (i = 0; i < 768; ++i) palette[i] = (unsigned char)(i * 73 + i / 7);
    for (i = 0; i < 256; ++i) {
        pixels[3 * i] = (unsigned char)i;
        pixels[3 * i + 1] = (unsigned char)(i * 29);
        pixels[3 * i + 2] = (unsigned char)(255 - i);
    }
    colors = 33;
    if (observation == 1) {
        /* Cell center is (15.5, 15.5, 15.5) for both widths. */
        memset(palette, 16, 6);
        palette[3] = 15;
        memset(pixels, 15, sizeof(pixels));
        colors = 2;
    }
    if (observation == 2) {
        memset(palette, 0, sizeof(palette));
        memset(palette + 765, 255, 3);
        memset(pixels, 255, sizeof(pixels));
        colors = 256;
    }
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.test_hooks = &hooks;
    request.dest = output;
    request.pixels = pixels;
    request.pixel_count = 256;
    request.width = request.height = 16;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = colors;
    request.lut_policy = bits == 5 ? SIXEL_LUT_POLICY_5BIT
        : SIXEL_LUT_POLICY_6BIT;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.bluenoise_strength_override = 1;
    request.bluenoise_strength = 1;
    request.bluenoise_phase_override = 1;
    request.bluenoise_channel_override = 1;
    request.bluenoise_size_override = 1;
    request.bluenoise_size = 64;
    request.positional_strength_override = 1;
    request.positional_strength = 1;
    if (observation == 12 || observation == 17 || observation == 18)
        request.method_for_diffuse = SIXEL_DIFFUSE_BLUENOISE_DITHER;
    if (observation == 13) request.method_for_diffuse = SIXEL_DIFFUSE_A_DITHER;
    if (observation == 14) request.method_for_diffuse = SIXEL_DIFFUSE_X_DITHER;
    if (observation == 19) {
        for (i = 0; i < 256; ++i) transparent[i] = i % 3 == 0;
        request.transparent_mask = transparent;
        request.transparent_mask_size = sizeof(transparent);
        request.transparent_keycolor = 255;
    }
    if (observation == 20 || observation == 21) {
        request.policy = SIXEL_GPU_POLICY_AUTO;
        request.auto_threshold = observation == 20 ? 0 : 257;
    }
    keep = observation >= 15 && observation <= 18;
    offset = 2 * 20 + 3;
    if (keep) {
        for (i = 0; i < 400; ++i) {
            valid[i] = i % 3 != 0;
            for (c = 0; c < 3; ++c)
                retained[i * 3 + c] = (unsigned char)(i * 19 + c * 53);
        }
        for (i = 0; i < 256; i += 2) {
            pos = offset + (i / 16) * 20 + i % 16;
            memcpy(retained + pos * 3, pixels + i * 3, 3);
            retained[pos * 3] ^= (unsigned char)(i % 5);
        }
        request.sixdelta_enabled = request.has_6delta_accumulation = 1;
        request.accumulation_pixels = retained;
        request.accumulation_pixels_size = sizeof(retained);
        request.accumulation_width = request.accumulation_height = 20;
        request.accumulation_origin_x = 3;
        request.accumulation_origin_y = 2;
        request.accumulation_valid_mask = valid;
        request.accumulation_valid_mask_size = sizeof(valid);
        request.accumulation_keycolor = 255;
        request.sixdelta_threshold = 8;
        request.accumulation_result_mask = mask;
        request.accumulation_result_mask_size = sizeof(mask);
    }
    status = sixel_gpu_palette_apply(&request);
    if (observation == 21)
        return status == SIXEL_FALSE && !hooks.entered ? 0 : 1;
    if (status != SIXEL_OK || hooks.kernel != rgb_cell_expected_kernel() ||
            !hooks.published) return 1;
    if (observation == 3) {
        if (hooks.kernel == 9) return rgb_cell_table_size(bits);
        return hooks.lookup_table_bytes == ((size_t)1U << (bits * 3)) ? 0 : 1;
    }
    if (observation >= 4 && observation <= 11) {
        memcpy(first, output, sizeof(first));
        if (observation == 5) palette[0] ^= 127;
        if (observation == 6) --request.ncolors;
        if (observation == 7) {
            request.lut_policy = bits == 5 ? SIXEL_LUT_POLICY_6BIT
                : SIXEL_LUT_POLICY_5BIT;
            /* The other policy must build its own table without evicting
             * the completed table retained by the original policy. */
            hooks.rebuild = -1;
            if (sixel_gpu_palette_apply(&request) != SIXEL_OK ||
                    hooks.rebuild != 1) return 1;
            request.lut_policy = bits == 5 ? SIXEL_LUT_POLICY_5BIT
                : SIXEL_LUT_POLICY_6BIT;
        }
        if (observation >= 8 && observation <= 10) {
            palette[0] ^= 127;
            hooks.fault = observation - 7;
            hooks.published = 0;
            if (sixel_gpu_palette_apply(&request) == SIXEL_OK ||
                    hooks.published) return 1;
            hooks.fault = 0;
            /* Return to the previously valid identity. A completed failed
             * rebuild may have overwritten its contents; it must not hit. */
            palette[0] ^= 127;
        }
        if (observation == 11) {
            for (i = 0; i < 128; ++i) {
                memcpy(swap, pixels + i * 3, 3);
                memcpy(pixels + i * 3, pixels + (255 - i) * 3, 3);
                memcpy(pixels + (255 - i) * 3, swap, 3);
            }
        }
        hooks.rebuild = -1;
        if (sixel_gpu_palette_apply(&request) != SIXEL_OK) return 1;
        if (observation == 4) return hooks.rebuild == 0 ? 0 : 1;
        if (observation == 7)
            return hooks.rebuild == 0 &&
                !memcmp(first, output, sizeof(first)) ? 0 : 1;
        if (observation == 11) {
            for (i = 0; i < 256; ++i)
                if (output[i] != first[255 - i]) return 1;
            return 0;
        }
        if (observation >= 8 && observation <= 10)
            return hooks.rebuild == 1 &&
                !memcmp(first, output, sizeof(first)) ? 0 : 1;
        return hooks.rebuild == 1 ? 0 : 1;
    }
    for (i = 0; i < 256; ++i) {
        rgb_cell_query(query, pixels + i * 3, i, request.method_for_diffuse);
        best = rgb_cell_reference(palette, colors, query, bits);
        want_mask = 0;
        if (keep) {
            pos = offset + (i / 16) * 20 + i % 16;
            within_threshold = 1;
            for (c = 0; c < 3; ++c) {
                delta = pixels[i * 3 + c] - retained[pos * 3 + c];
                if (delta < -8 || delta > 8) within_threshold = 0;
            }
            if (valid[pos] && within_threshold) {
                want_mask = memcmp(pixels + i * 3, retained + pos * 3, 3)
                    ? 3 : 1;
            } else if (valid[pos] &&
                    rgb_cell_distance(pixels + i * 3, retained + pos * 3) <=
                    rgb_cell_distance(pixels + i * 3, palette + best * 3)) {
                want_mask = 1;
            }
            if (want_mask) best = 255;
        }
        if (observation == 19 && transparent[i]) best = 255;
        if (observation == 16 || observation == 18) {
            if (mask[i] != want_mask) return 1;
        } else if ((int)output[i] != best) {
            return 1;
        }
    }
    return 0;
}

static int
rgb_cell_stream_write(char *data, int size, void *context)
{
    size_t *count;

    (void)data;
    count = context;
    if (size < 0) return -1;
    *count += (size_t)size;
    return size;
}

/* Ownership is observed after a complete encode, not inferred from the
 * selected option. Separate dither objects must share only the GPU table. */
int
test_rgb_cell_encode(int bits, int observation)
{
    sixel_dither_t *dither;
    sixel_output_t *output;
    sixel_gpu_palette_test_hooks_t hooks;
    unsigned char palette[9], pixels[16 * 16 * 3];
    size_t count;
    int pass, ok, reuse;
    SIXELSTATUS status;

    dither = NULL;
    output = NULL;
    ok = 0;
    count = 0;
    memset(&hooks, 0, sizeof(hooks));
    /* Keep a small non-monochrome palette for the ownership probe. */
    memset(palette, 0, 3);
    memset(palette + 3, 127, 3);
    memset(palette + 6, 255, 3);
    memset(pixels, 32, sizeof(pixels));
    reuse = observation == 2 || observation == 3;
    for (pass = 0; pass <= reuse; ++pass) {
        if (sixel_dither_new(&dither, 3, NULL) != SIXEL_OK ||
                sixel_output_new(&output, rgb_cell_stream_write, &count,
                    NULL) != SIXEL_OK) goto end;
        sixel_dither_set_palette(dither, palette);
        sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
        sixel_dither_set_lut_policy(dither, bits == 5
            ? SIXEL_LUT_POLICY_5BIT : SIXEL_LUT_POLICY_6BIT);
        sixel_dither_set_diffusion_type(dither, SIXEL_DIFFUSE_NONE);
        sixel_dither_set_optimize_palette(dither, 0);
        dither->gpu_policy = observation == 1 || observation == 4
            ? SIXEL_GPU_POLICY_AUTO : SIXEL_GPU_POLICY_FORCE;
        dither->gpu_palette_threshold = observation == 4 ? 1000000U : 0U;
        /* Hooks select the legacy adapter. Ownership must instead
         * exercise the normal indexed component path without hooks. */
        dither->gpu_test_hooks = reuse ? &hooks : NULL;
        if (observation == 2) {
            dither->lut_policy_shared_instance_override = 1;
            dither->lut_policy_shared_instance = pass;
        }
        if (observation == 3) {
            dither->lut_policy_packing_override = 1;
            dither->lut_policy_packing = pass ? SIXEL_LOOKUP_PACK_LINEAR
                : SIXEL_LOOKUP_PACK_MORTON;
        }
        status = sixel_encode(pixels, 16, 16, 3, dither, output);
        if (status != SIXEL_OK || count == 0 ||
                (!reuse && output->indexed_count != 1)) {
            fprintf(stderr, "encode status=%d bytes=%lu indexed=%lu: %s\n",
                status, (unsigned long)count,
                (unsigned long)output->indexed_count,
                sixel_helper_get_additional_message());
            goto end;
        }
        if (observation == 4) {
            ok = dither->lookup_policy != NULL;
        } else if (reuse) {
            if (hooks.kernel != rgb_cell_expected_kernel() ||
                    !hooks.published) {
                fprintf(stderr, "kernel=%d published=%d\n", hooks.kernel,
                    hooks.published);
                goto end;
            }
            ok = pass && hooks.rebuild == 0;
        } else {
            ok = dither->lookup_policy == NULL;
        }
        sixel_output_unref(output);
        sixel_dither_unref(dither);
        output = NULL;
        dither = NULL;
    }
end:
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? 0 : 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
