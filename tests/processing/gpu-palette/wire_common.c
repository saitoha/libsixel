/* SPDX-License-Identifier: MIT */
/* Independent emitted-color witnesses cover the output-owner/GPU boundary. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/output.h"
#include "src/palette.h"
#include "src/gpu-palette.h"
#include "src/sixel_decode_pixels.h"

typedef struct gpu_wire_stream {
    unsigned char bytes[4096];
    size_t size;
} gpu_wire_stream_t;

static int
gpu_wire_write(char *bytes, int size, void *context)
{
    gpu_wire_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size > sizeof(stream->bytes) - stream->size) {
        return -1;
    }
    memcpy(stream->bytes + stream->size, bytes, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

int
test_gpupal_wire(int method, int hls, int lookup)
{
    /* RGB: raw [11,11,6] emits [10,10,5]. At target [13,13,8], keeping
     * [10,10,8] costs 18; painting costs 27, not the raw candidate's 12.
     * HLS: [31,116,201] emits (330,45,73), decoded as [31,115,199].
     * Integer keep/paint expectations are independent of production math. */
    unsigned char palette[9] = {11, 11, 6, 200, 200, 200, 0, 0, 0};
    unsigned char const hls_entry[3] = {31, 116, 201};
    unsigned char const wire_rgb[3] = {10, 10, 5};
    unsigned char const wire_hls[3] = {31, 115, 199};
    unsigned char source[15], history[15];
    unsigned char valid[5] = {1, 1, 1, 1, 0};
    unsigned char expected[5] = {1, 1, 0, 1, 0};
    unsigned char *decoded, *paint, *decoded_palette;
    unsigned char const *mask, *wire;
    sixel_dither_t *dither;
    sixel_output_t *output;
    sixel_palette_entries_view_t view;
    gpu_wire_stream_t stream;
    unsigned int flags;
    size_t size;
    int pass, i, width, height, colors, ok;

    dither = NULL;
    output = NULL;
    decoded = paint = decoded_palette = NULL;
    ok = 0;
    if (hls) memcpy(palette, hls_entry, 3);
    wire = hls ? wire_hls : wire_rgb;
    memset(source, 0, sizeof(source));
    memset(history, 0, sizeof(history));
    for (i = 0; i < 5; ++i) memcpy(source + i * 3, palette, 3);
    memcpy(history, wire, 3);
    memcpy(history + 3, wire, 3);
    memcpy(history + 9, palette, 3);
    if (hls) {
        memcpy(source + 3, wire, 3);
        memcpy(history + 3, palette, 3);
        expected[1] = 0;
    } else {
        source[0] = source[1] = 13;
        source[2] = history[2] = 8;
    }
    if (SIXEL_FAILED(sixel_dither_new(&dither, 3, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, gpu_wire_write,
                &stream, NULL))) goto end;
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_transparent(dither, 2);
    dither->keycolor_reserved = 1;
    dither->force_palette = 1;
    dither->bluenoise_strength_override = 1;
    dither->bluenoise_strength = 0.5f;
    dither->a_dither_strength_override = dither->x_dither_strength_override = 1;
    dither->a_dither_strength = dither->x_dither_strength = 0.5f;
    sixel_dither_set_pipeline_accumulation_result_enabled(dither, 1);
    sixel_dither_set_lut_policy(dither, lookup);
    sixel_dither_set_diffusion_type(dither, method);
    sixel_output_set_palette_type(output,
        hls ? SIXEL_PALETTETYPE_HLS : SIXEL_PALETTETYPE_RGB);
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(dither,
            SIXEL_DITHER_CLAMP_HARD, 1.01))) goto end;
    for (pass = 0; pass < 2; ++pass) {
        /* FORCE reports errors instead of silently falling back to CPU. */
        dither->gpu_policy = pass ? SIXEL_GPU_POLICY_FORCE
            : SIXEL_GPU_POLICY_OFF;
        stream.size = 0;
        sixel_dither_set_pipeline_accumulation_buffer_hint(dither,
            history, sizeof(history), valid, sizeof(valid),
            5, 1, 0, 0, 5, 1, 2, 1, 0, SIXEL_6DELTA_ERROR_SKIP);
        if (SIXEL_FAILED(sixel_encode(source, 5, 1, 1, dither, output)) ||
                dither->pipeline_6delta_rgb101 ||
                dither->pipeline_6delta_hls_bytes != NULL ||
                SIXEL_FAILED(dither->palette->vtbl->get_entries(
                    dither->palette, &view)) ||
                memcmp(view.entries, palette, sizeof(palette))) goto end;
        mask = sixel_dither_get_pipeline_accumulation_result_mask(dither,
            &size);
        if (mask == NULL || size != 5 || memcmp(mask, expected, 5)) {
            fprintf(stderr, "wire keep mismatch gpu=%d hls=%d method=%d\n",
                pass, hls, method);
            goto end;
        }
        if (SIXEL_FAILED(sixel_decode_raw_with_options_mask(stream.bytes,
                (int)stream.size, 0, &decoded, &paint, &width, &height,
                &decoded_palette, &colors, &flags, dither->allocator)) ||
                width != 5 || height != 1) goto end;
        for (i = 0; i < 5; ++i) {
            if (!!paint[i] == !!expected[i]) goto end;
            if (paint[i] && (decoded[i] >= colors ||
                    memcmp(decoded_palette + decoded[i] * 3, wire, 3))) {
                goto end;
            }
        }
        sixel_allocator_free(dither->allocator, decoded);
        sixel_allocator_free(dither->allocator, paint);
        sixel_allocator_free(dither->allocator, decoded_palette);
        decoded = paint = decoded_palette = NULL;
    }
    ok = 1;
end:
    if (!ok) fprintf(stderr, "GPU wire comparison: %s\n",
        sixel_helper_get_additional_message());
    if (dither != NULL) {
        sixel_allocator_free(dither->allocator, decoded);
        sixel_allocator_free(dither->allocator, paint);
        sixel_allocator_free(dither->allocator, decoded_palette);
    }
    sixel_output_unref(output);
    sixel_dither_unref(dither);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* A standalone caller has no wire format. An optional view must be complete
 * before the GPU can read it, even when the raw lookup palette is valid. */
int
test_gpupal_comparison_request(int short_view)
{
    unsigned char source[3] = {13, 13, 8};
    unsigned char history[3] = {10, 10, 8};
    unsigned char palette[3] = {11, 11, 6};
    unsigned char wire[3] = {10, 10, 5};
    unsigned char valid = 1, result = 0;
    sixel_index_t index;
    sixel_gpu_palette_request_t request;
    SIXELSTATUS status;

    memset(&request, 0, sizeof(request));
    request.policy = SIXEL_GPU_POLICY_FORCE;
    request.dest = &index;
    request.pixels = source;
    request.pixel_count = 1;
    request.width = request.height = 1;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.palette = palette;
    request.palette_size = sizeof(palette);
    request.palette_depth = 3;
    request.ncolors = 1;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;
    request.method_for_diffuse = SIXEL_DIFFUSE_NONE;
    request.method_for_scan = SIXEL_SCAN_RASTER;
    request.sixdelta_enabled = request.has_6delta_accumulation = 1;
    request.accumulation_pixels = history;
    request.accumulation_pixels_size = sizeof(history);
    request.accumulation_valid_mask = &valid;
    request.accumulation_valid_mask_size = 1;
    request.accumulation_keycolor = 1;
    request.accumulation_result_mask = &result;
    request.accumulation_result_mask_size = 1;
    if (short_view) {
        request.sixdelta_comparison_palette = wire;
        request.sixdelta_comparison_palette_size = 2;
    }
    status = sixel_gpu_palette_apply(&request);
    if (short_view) return status == SIXEL_BAD_ARGUMENT ? 0 : 1;
    return status == SIXEL_OK && index == 0 && result == 0 ? 0 : 1;
}
