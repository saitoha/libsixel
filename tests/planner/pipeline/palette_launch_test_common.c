/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "palette_launch_test_common.h"

int
palette_launch_input(sixel_allocator_t *allocator, int floating,
    sixel_palette_sampling_policy_t sampling, sixel_encoder_t **encoder,
    sixel_frame_t **source, sixel_palette_build_request_t **request,
    sixel_frame_t **sample)
{
    sixel_palette_frame_state_t state;
    unsigned char *pixels;
    float *native;
    int i, format;
    SIXELSTATUS status;

    *encoder = NULL;
    *source = NULL;
    *request = NULL;
    *sample = NULL;
    format = floating ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888;
    if (sixel_encoder_new(encoder, allocator) != SIXEL_OK ||
            sixel_frame_new(source, allocator) != SIXEL_OK) {
        return 1;
    }
    pixels = sixel_allocator_malloc(allocator,
        (size_t)768 * (floating ? sizeof(float) : 1));
    if (pixels == NULL) {
        return 1;
    }
    native = (float *)(void *)pixels;
    for (i = 0; i < 768; ++i) {
        if (floating) {
            native[i] = (float)(i % 17) / 16.0f;
        } else {
            pixels[i] = (unsigned char)((i % 17) * 15);
        }
    }
    if (sixel_frame_init(*source, pixels, 16, 16, format, NULL, -1)
            != SIXEL_OK) {
        return 1;
    }
    (*encoder)->reqcolors = 16;
    (*encoder)->lut_policy = SIXEL_LUT_POLICY_NONE;
    (*encoder)->lut_policy_override = 1;
    (*encoder)->planner.working_colorspace_effective = SIXEL_COLORSPACE_GAMMA;
    (*encoder)->planner.working_pixelformat = format;
    sixel_palette_frame_state_init(&state);
    sixel_palette_policy_resolution_init(&state.quantizer,
        (*encoder)->quantize_model, SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
    sixel_palette_binning_state_init(&state.binning,
        SIXEL_PALETTE_BINNING_AUTO, SIXEL_PALETTE_POLICY_ORIGIN_DEFAULT);
    sixel_palette_policy_resolution_init(&state.sampling, sampling,
        SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    if (sixel_palette_sampling_resolve(&state, sampling,
            sampling == SIXEL_PALETTE_SAMPLING_FULL_FRAME
                ? SIXEL_PALETTE_SAMPLING_SOURCE_PREPROCESSED_FRAME
                : SIXEL_PALETTE_SAMPLING_SOURCE_LOADED_FRAME,
            SIXEL_PALETTE_RESOLUTION_EXPLICIT) != SIXEL_OK) {
        return 1;
    }
    state.delta_alpha_key = 0;
    status = sixel_palette_build_input_new(request, sample, *encoder, *source,
        &state);
    if (status != SIXEL_OK) {
        fprintf(stderr, "palette input: %x %s\n", status,
            sixel_helper_get_additional_message());
    }
    return status != SIXEL_OK;
}

int
palette_launch_equal(sixel_dither_t *a, sixel_dither_t *b, int floating)
{
    sixel_palette_entries_view_t bytes_a, bytes_b;
    sixel_palette_float32_entries_view_t float_a, float_b;

    if (a->ncolors != b->ncolors || a->keycolor != b->keycolor) {
        return 0;
    }
    if (floating) {
        a->palette->vtbl->get_entries_float32(a->palette, &float_a);
        b->palette->vtbl->get_entries_float32(b->palette, &float_b);
        return float_a.entries != NULL && float_b.entries != NULL &&
            !memcmp(float_a.entries, float_b.entries,
                (size_t)a->ncolors * 3 * sizeof(float));
    }
    a->palette->vtbl->get_entries(a->palette, &bytes_a);
    b->palette->vtbl->get_entries(b->palette, &bytes_b);
    return !memcmp(bytes_a.entries, bytes_b.entries, (size_t)a->ncolors * 3);
}

SIXELSTATUS
palette_launch_clone(sixel_frame_t *frame, sixel_allocator_t *allocator,
    sixel_frame_t **out)
{
    sixel_frame_interface_t *source, *copy;
    SIXELSTATUS status;

    source = sixel_frame_as_interface(frame);
    copy = NULL;
    status = source->vtbl->clone(source, allocator, &copy);
    *out = (sixel_frame_t *)copy;
    return status;
}
