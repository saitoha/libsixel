/* SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/README.md
 * Policy: docs/encoder/palette-application/dithering/README.md
 * Compare bulk rows with masked, clipped and retained-plane point rows.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/dither.h"
#include "dither-policy-private.h"
#include "src/factory.h"
#include "src/palette.h"

static int none_batch_rows;
static int none_batch_order;

static void
none_batch_row(void *priv, int row)
{
    (void)priv;
    if (row != none_batch_rows) {
        none_batch_order = 0;
    }
    ++none_batch_rows;
}

int
test_lookup_0017_none_batch_fences(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *lookup;
    sixel_dither_policy_interface_t *policy;
    sixel_lookup_policy_prepare_request_t lp;
    sixel_dither_policy_prepare_request_t dp;
    sixel_dither_policy_apply_request_t request;
    sixel_dither_execution_t request_execution;
    sixel_palette_float32_entries_request_t entries;
    unsigned char palette[12] = { 0, 0, 0, 100, 100, 100,
                                 200, 200, 200, 255, 255, 255 };
    float palette_float[12];
    unsigned char pixels[51 * 3];
    float pixels_float[51 * 3];
    float expected_float[51 * 3];
    unsigned char original[51 * 3];
    unsigned char mask[51];
    unsigned char valid[51];
    sixel_index_t result[51];
    sixel_index_t expected[51];
    int f;
    int scan;
    int mode;
    int i;
    int ok;
    SIXELSTATUS status;

    (void)argc;
    (void)argv;
    allocator = NULL;
    dither = NULL;
    factory = NULL;
    lookup = NULL;
    policy = NULL;
    ok = 0;
    memset(&lp, 0, sizeof(lp));
    memset(&dp, 0, sizeof(dp));
    memset(&request, 0, sizeof(request));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))
            || SIXEL_FAILED(sixel_dither_new(&dither, 4, allocator))
            || SIXEL_FAILED(sixel_components_getservice("services/factory",
                                                     (void **)&factory))) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    dither->method_for_diffuse = SIXEL_DIFFUSE_NONE;
    dither->pipeline_row_callback = none_batch_row;
    for (i = 0; i < 12; ++i) {
        palette_float[i] = (float)palette[i] / 255.0f;
    }
    for (i = 0; i < 51 * 3; ++i) {
        original[i] = (unsigned char)(i * 37);
    }
    lp.palette = palette;
    lp.depth = 3;
    lp.reqcolor = 4;
    lp.allocator = allocator;
    request.execution = &request_execution;
    request.result = result;
    request.width = 17;
    request.height = 3;
    request.depth = 3;
    request.palette = palette;
    for (f = 0; f < 2; ++f) {
        dither->use_rgb101_palette = f;
        if (f) {
            memset(&entries, 0, sizeof(entries));
            entries.entries = palette_float;
            entries.colors = 4;
            entries.depth = 3 * (int)sizeof(float);
            if (SIXEL_FAILED(dither->palette->vtbl->init_entries_float32(
                    dither->palette, &entries))
                    || SIXEL_FAILED(sixel_palette_prepare_rgb101(
                        dither->palette, SIXEL_COLORSPACE_GAMMA,
                        SIXEL_PALETTE_OUTPUT_LEGACY))) {
                fprintf(stderr, "native output palette preparation failed\n");
                goto end;
            }
        }
        lp.pixelformat = f ? SIXEL_PIXELFORMAT_RGBFLOAT32
                          : SIXEL_PIXELFORMAT_RGB888;
        lp.palette_float = f ? palette_float : NULL;
        lp.float_depth = f ? 3 * (int)sizeof(float) : 0;
        dp.pixelformat = lp.pixelformat;
        status = factory->vtbl->create(factory, f ? "lookup/vptree.float32"
                                                 : "lookup/vptree.8bit",
                                      allocator, (void **)&lookup);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = factory->vtbl->create(factory, f ? "dither/none.float32"
                                                 : "dither/none.8bit",
                                      allocator, (void **)&policy);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        request.lookup_policy = lookup;
        for (scan = 0; scan < 2; ++scan) {
            dp.method_for_scan = scan ? SIXEL_SCAN_SERPENTINE
                                      : SIXEL_SCAN_RASTER;
            sixel_dither_policy_capture_options(dither, &dp.options);
            if (SIXEL_FAILED(policy->vtbl->prepare(policy, &dp))) {
                goto end;
            }
            for (mode = 0; mode < 6; ++mode) {
                memcpy(pixels, original, sizeof(pixels));
                for (i = 0; i < 51 * 3; ++i) {
                    pixels_float[i] = (float)original[i] / 255.0f;
                }
                memset(mask, 0, sizeof(mask));
                memset(valid, mode == 5 ? 1 : 0, sizeof(valid));
                memset(result, 0xfe, sizeof(result));
                request.data = f ? (unsigned char *)(void *)pixels_float
                                 : pixels;
                request.output_start = mode == 2 ? 1 : 0;
                dither->pipeline_transparent_mask = mode == 1 || mode == 3
                    ? mask : NULL;
                dither->pipeline_transparent_mask_size = sizeof(mask);
                dither->pipeline_transparent_keycolor = 3;
                mask[19] = mode == 3 ? 1 : 0;
                sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
                    original, sizeof(original), valid, sizeof(valid),
                    17, 3, 0, 0, 17, 3, 3, mode >= 4, 0,
                    SIXEL_6DELTA_ERROR_DIFFUSE);
                if (SIXEL_FAILED(lookup->vtbl->prepare(lookup, &lp))) {
                    goto end;
                }
                none_batch_rows = request.output_start;
                none_batch_order = 1;
                sixel_dither_policy_capture_execution(dither,
                    &request_execution);
                if (SIXEL_FAILED(policy->vtbl->apply(policy, &request))
                        || none_batch_rows != 3 || !none_batch_order) {
                    goto end;
                }
                if (mode == 0) {
                    memcpy(expected, result, sizeof(result));
                    memcpy(expected_float, pixels_float, sizeof(pixels_float));
                }
                for (i = 0; i < 51; ++i) {
                    if (result[i] != (mode == 2 && i < 17 ? 0xfe
                            : mode == 5 || (mode == 3 && i == 19) ? 3
                            : expected[i])) {
                        fprintf(stderr, "none f=%d mode=%d pixel=%d\n",
                                f, mode, i);
                        goto end;
                    }
                }
                if (f && (mode == 1 || mode == 4)
                        && memcmp(pixels_float, expected_float,
                                  sizeof(pixels_float)) != 0) {
                    goto end;
                }
            }
        }
        policy->vtbl->unref(policy);
        lookup->vtbl->unref(lookup);
        policy = NULL;
        lookup = NULL;
    }
    ok = 1;
end:
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    if (lookup != NULL) {
        lookup->vtbl->unref(lookup);
    }
    if (factory != NULL) {
        factory->vtbl->unref(factory);
    }
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
