/* SPDX-License-Identifier: MIT */
/* Shared setup and numeric observations, without a native-output cache. */
#include "retention_test_common.h"

int
retention_write(char *data, int size, void *context)
{
    retention_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >= sizeof(capture->bytes) - capture->length) {
        return -1;
    }
    memcpy(capture->bytes + capture->length, data, (size_t)size);
    capture->length += (size_t)size;
    capture->bytes[capture->length] = '\0';
    return size;
}

int
retention_init(sixel_dither_t **dither, void *pixels, int format, int model)
{
    sixel_palette_build_attempt_t attempt;
    SIXELSTATUS status;

    if (SIXEL_FAILED(sixel_dither_new(dither, 2, NULL))) {
        return 0;
    }
    (*dither)->quantize_model = model;
    sixel_dither_set_diffusion_type(*dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_lut_policy(*dither, SIXEL_LUT_POLICY_NONE);
    sixel_dither_set_final_merge(*dither, SIXEL_FINAL_MERGE_NONE);
    memset(&attempt, 0, sizeof(attempt));
    sixel_palette_binning_state_init(&attempt.binning,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    if (model == SIXEL_QUANTIZE_MODEL_KCENTER) {
        status = sixel_palette_binning_resolve(&attempt.binning,
            SIXEL_PALETTE_BINNING_NONE, 0, SIXEL_PALETTE_BINNING_GRID_NONE,
            SIXEL_PALETTE_BINNING_KERNEL_NONE,
            SIXEL_PALETTE_BINNING_BACKEND_DIRECT, 2,
            SIXEL_PALETTE_RESOLUTION_EXPLICIT);
        if (SIXEL_FAILED(status)) {
            return 0;
        }
    }
    status = sixel_dither_initialize_with_palette_attempt(*dither, pixels,
        2, 1, format, SIXEL_LARGE_AUTO, SIXEL_REP_AUTO,
        SIXEL_QUALITY_FULL, &attempt);
    /* A fallback or native route cannot stand in for the requested case. */
    return SIXEL_SUCCEEDED(status) && attempt.quantize_model == model &&
        attempt.quantizer_retry_count == 0 &&
        !(*dither)->use_rgb101_palette && (*dither)->ncolors == 2;
}

int
retention_pair(sixel_dither_t *dither, float const expected[6])
{
    sixel_palette_float32_entries_view_t view;
    unsigned int first;
    unsigned int row;
    unsigned int axis;
    float value;

    memset(&view, 0, sizeof(view));
    if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
            dither->palette, &view)) || view.entries == NULL ||
            view.entry_count != 2 || view.depth != 3 * (int)sizeof(float)) {
        return 0;
    }
    first = view.entries[0] < view.entries[3] ? 0u : 1u;
    for (row = 0; row < 2; ++row) {
        for (axis = 0; axis < 3; ++axis) {
            value = view.entries[((first + row) % 2) * 3 + axis];
            /* Allows solver scale/unscale and binary32 storage error.
             * Fixtures are much farther from any byte-derived coordinate. */
            if (!isfinite(value) ||
                    fabs((double)value - expected[row * 3 + axis]) > 2e-6) {
                return 0;
            }
        }
    }
    return 1;
}

int
retention_pick(sixel_dither_t *dither, float const pixel[3], int slot)
{
    float copy[3];
    sixel_index_t *indices;
    int ok;

    memcpy(copy, pixel, sizeof(copy));
    indices = sixel_dither_apply_palette(dither, (unsigned char *)copy, 1, 1);
    ok = indices != NULL && indices[0] == slot;
    sixel_allocator_free(dither->allocator, indices);
    return ok;
}

int
retention_generated(int model)
{
    float pixels[6] = {.0149f, .0149f, .0149f, .5149f, .5149f, .5149f};
    sixel_dither_t *dither;
    int ok;

    dither = NULL;
    ok = retention_init(&dither, pixels, SIXEL_PIXELFORMAT_RGBFLOAT32, model)
        && retention_pair(dither, pixels);
    sixel_dither_unref(dither);
    return ok;
}

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
unsigned char *
retention_bytes(sixel_dither_t *dither)
{
    /* Exercise the actual legacy mutable getter, not the component view. */
    return sixel_dither_get_palette(dither);
}
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
