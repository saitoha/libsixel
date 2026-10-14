/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"
#include "src/delta-residual.h"
#include "src/sixel_decode_pixels.h"

#define RESIDUAL_REQUIRE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "residual assertion at %d: %s (%s)\n", \
            __LINE__, #expr, sixel_helper_get_additional_message()); \
        goto end; \
    } \
} while (0)

typedef struct residual_test_output {
    unsigned char data[16384];
    size_t size;
    int fail;
} residual_test_output_t;

static int
residual_test_write(char *data, int size, void *priv)
{
    residual_test_output_t *out;

    out = priv;
    if (out->fail || size < 0 ||
            (size_t)size > sizeof(out->data) - out->size) return -1;
    memcpy(out->data + out->size, data, (size_t)size);
    out->size += (size_t)size;
    return size;
}

/* Each call supplies fresh source storage; encode may preprocess the frame.
 * Deliberately leave frame_no at its default, as ordinary API clients may. */
static SIXELSTATUS
residual_test_encode(sixel_encoder_t *encoder, residual_test_output_t *out)
{
    sixel_frame_t *frame;
    sixel_output_t *output;
    unsigned char *pixels;
    SIXELSTATUS status;

    frame = NULL;
    output = NULL;
    pixels = malloc(16U * 16U * 3U);
    if (pixels == NULL) return SIXEL_BAD_ALLOCATION;
    memset(pixels, 108, 16U * 16U * 3U);
    status = sixel_frame_new(&frame, NULL);
    if (status != SIXEL_OK) {
        free(pixels);
        return status;
    }
    status = sixel_frame_init(frame, pixels, 16, 16,
        SIXEL_PIXELFORMAT_RGB888, NULL, 0);
    if (status == SIXEL_OK) {
        status = sixel_output_new(&output, residual_test_write, out, NULL);
    }
    out->size = 0;
    if (status == SIXEL_OK) {
        status = sixel_encoder_encode_frame(encoder, frame, output);
    }
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    return status;
}

static int
residual_test_decoded(residual_test_output_t const *out,
    sixel_encoder_t *encoder)
{
    unsigned char *pixels, *mask, *palette;
    int width, height, colors, ok;
    unsigned int flags;
    size_t i;

    pixels = mask = palette = NULL;
    ok = 0;
    if (sixel_decode_raw_with_options_mask((unsigned char *)out->data,
            (int)out->size, 0, &pixels, &mask, &width, &height,
            &palette, &colors, &flags, encoder->allocator) != SIXEL_OK ||
            width != 16 || height != 16) goto end;
    for (i = 0; i < 256; ++i) {
        if (!mask[i] || pixels[i] >= colors ||
                memcmp(palette + pixels[i] * 3,
                       encoder->accumulation_pixels + i * 3, 3) ||
                palette[pixels[i] * 3] <= 102 ||
                palette[pixels[i] * 3] >= 114) goto end;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "decode=%dx%d colors=%d acc=%d stream=%.*s\n",
            width, height, colors, encoder->accumulation_pixels[0],
            (int)out->size, out->data);
    }
    sixel_allocator_free(encoder->allocator, pixels);
    sixel_allocator_free(encoder->allocator, mask);
    sixel_allocator_free(encoder->allocator, palette);
    return ok;
}

int
test_residual_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_delta_residual_t state;
    residual_test_output_t out;
    sixel_progressive_t *progressive;
    sixel_progressive_config_t config;
    unsigned char source[768], acc[768], valid[256];
    unsigned char a[3], b[3];
    unsigned char saved_age[4096];
    int ok, i;

    encoder = NULL;
    progressive = NULL;
    ok = 0;
    memset(&out, 0, sizeof(out));
    memset(&state, 0, sizeof(state));
    memset(acc, 102, sizeof(acc));
    memset(source, 108, sizeof(source));
    memset(valid, 1, sizeof(valid));
    RESIDUAL_REQUIRE(sixel_encoder_new(&encoder, NULL) == SIXEL_OK);
    RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_DIFFUSION, "none") == SIXEL_OK);
    RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_GPU_POLICY, "off") == SIXEL_OK);
    if (mode == 1) {
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta") == SIXEL_OK);
        RESIDUAL_REQUIRE(!encoder->residual_enabled &&
            encoder->sixdelta_threshold == 0 && encoder->residual_frames == 3 &&
            encoder->residual_threshold == 1.5 &&
            encoder->residual_keep_threshold == 0 &&
            encoder->sixdelta_metric == SIXEL_DELTA_METRIC_AUTO);
        ok = 1;
        goto end;
    }
    RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
        SIXEL_OPTFLAG_UPDATE_POLICY, "delta:R1:T8") == SIXEL_OK);
    RESIDUAL_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, acc,
        16, 16, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
    switch (mode) {
    case 2:
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY,
            "delta:threshold_metric=r:residual=1:threshold=8:"
            "residual_threshold=1.5:residual_keep_threshold=0:"
            "residual_frames=3")
                == SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->residual_enabled &&
            encoder->sixdelta_metric == SIXEL_DELTA_METRIC_RGB241 &&
            encoder->residual_threshold == 1.5 &&
            encoder->residual_frames == 3);
        break;
    case 3:
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
        memcpy(saved_age, encoder->residual_state.age, sizeof(saved_age));
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta:R1:T2:F0") != SIXEL_OK);
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta:R1:P1") != SIXEL_OK);
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta:R1:Dmax") != SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->sixdelta_threshold == 8 &&
            encoder->accumulation_valid && encoder->residual_enabled &&
            !memcmp(saved_age, encoder->residual_state.age, sizeof(saved_age)));
        break;
    case 4:
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_WORKING_COLORSPACE, "linear") != SIXEL_OK);
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_DIFFUSION, "fs") != SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->working_colorspace == SIXEL_COLORSPACE_GAMMA
            && encoder->method_for_diffuse == SIXEL_DIFFUSE_NONE &&
            encoder->accumulation_valid);
        break;
    case 5:
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->residual_state.age[0] == 1);
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta:R1:T8:Drgb241") == SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->residual_state.age[0] == 1);
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta:R1:T8:H2") == SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->residual_state.age[0] == 0 &&
            encoder->accumulation_valid);
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta") == SIXEL_OK);
        RESIDUAL_REQUIRE(!encoder->residual_enabled &&
            encoder->residual_threshold == 1.5);
        break;
    case 6:
        memset(a, 100, 3);
        memset(b, 100, 3);
        a[2] = 121;
        RESIDUAL_REQUIRE(sixel_delta_rgb241_within(a, b, 8));
        a[2] = 122;
        RESIDUAL_REQUIRE(!sixel_delta_rgb241_within(a, b, 8));
        a[2] = 100; a[1] = 111;
        RESIDUAL_REQUIRE(!sixel_delta_rgb241_within(a, b, 8));
        a[1] = 100; a[0] = 114;
        RESIDUAL_REQUIRE(sixel_delta_rgb241_within(a, b, 8));
        a[0] = 115;
        RESIDUAL_REQUIRE(!sixel_delta_rgb241_within(a, b, 8));
        memset(a, 108, 3);
        RESIDUAL_REQUIRE(sixel_delta_rgb241_within(a, b, 8));
        RESIDUAL_REQUIRE(!sixel_delta_rgb241_within(a, b, 7));
        break;
    case 7:
        for (i = 0; i < 256; ++i) {
            memset(source + i * 3, i % 2 ? 110 : 94, 3);
        }
        sixel_delta_residual_prepare(&state, source, acc, valid,
            128, 2, 1.5, 1, 0);
        RESIDUAL_REQUIRE(state.next_age[32] == 0);
        memset(source, 105, sizeof(source));
        sixel_delta_residual_prepare(&state, source, acc, valid,
            128, 2, 1.5, 1, 0);
        RESIDUAL_REQUIRE(state.selected_pixels == 256);
        valid[65] = 0;
        sixel_delta_residual_prepare(&state, source, acc, valid,
            128, 2, 1.5, 1, 0);
        RESIDUAL_REQUIRE(state.next_age[32] == 0);
        break;
    case 8:
        memset(source, 103, sizeof(source));
        sixel_delta_residual_prepare(&state, source, acc, valid,
            16, 16, 1.0, 1, 0);
        RESIDUAL_REQUIRE(state.selected_pixels == 0);
        sixel_delta_residual_prepare(&state, source, acc, valid,
            16, 16, 0.999, 1, 0);
        RESIDUAL_REQUIRE(state.selected_pixels == 256);
        break;
    case 9:
        state.mask.width = 65; state.mask.height = 1;
        state.mask.columns = 64; state.mask.rows = 1;
        state.mask.cells[1] = (uint32_t)1 << 31;
        state.mask.threshold = 2;
        RESIDUAL_REQUIRE(sixel_delta_mask_threshold(&state.mask, 62, 0, 8)
            == 8);
        RESIDUAL_REQUIRE(sixel_delta_mask_threshold(&state.mask, 63, 0, 8)
            == 2);
        RESIDUAL_REQUIRE(sixel_delta_mask_threshold(&state.mask, 64, 0, 8)
            == 2);
        RESIDUAL_REQUIRE(sixel_delta_mask_threshold(&state.mask, 64, 0, 1)
            == 1);
        break;
    case 10:
    case 11:
    case 12:
    case 23:
    case 24:
    case 25:
    case 26:
        if (mode == 23 || mode == 25) {
            RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_DIFFUSION, "a_dither") == SIXEL_OK);
        } else if (mode == 24 || mode == 26) {
            RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_DIFFUSION, "x_dither") == SIXEL_OK);
        }
        if (mode != 10 && mode != 23 && mode != 24) {
            RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_GPU_POLICY, "force") == SIXEL_OK);
        }
        if (mode == 12) {
            RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
                SIXEL_OPTFLAG_DIFFUSION, "bluenoise") == SIXEL_OK);
        }
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_LUT_POLICY, "none") == SIXEL_OK);
        for (i = 1; i <= 3; ++i) {
            RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
            RESIDUAL_REQUIRE(encoder->residual_state.age[0] == i);
            RESIDUAL_REQUIRE(sixel_encoder_get_6delta_painted_pixels(encoder)
                == (i == 3 ? 256U : 0U));
        }
        RESIDUAL_REQUIRE(residual_test_decoded(&out, encoder));
        break;
    case 13:
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
        RESIDUAL_REQUIRE(sixel_encoder_set_6delta_threshold_limit(encoder, 0)
            == SIXEL_OK);
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->residual_state.age[0] == 0 &&
            encoder->sixdelta_threshold == 8 &&
            sixel_encoder_get_6delta_painted_pixels(encoder) == 256);
        RESIDUAL_REQUIRE(residual_test_decoded(&out, encoder));
        break;
    case 14:
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
        RESIDUAL_REQUIRE(sixel_encoder_set_accumulation_buffer(encoder, acc,
            16, 16, SIXEL_PIXELFORMAT_RGB888) == SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->residual_state.age[0] == 0);
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
        RESIDUAL_REQUIRE(sixel_encoder_invalidate_6delta_plane(encoder)
            == SIXEL_OK);
        RESIDUAL_REQUIRE(encoder->residual_state.age[0] == 0 &&
            !encoder->accumulation_valid);
        break;
    case 15:
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
        out.fail = 1;
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) != SIXEL_OK);
        RESIDUAL_REQUIRE(!encoder->accumulation_valid &&
            encoder->residual_state.age[0] == 0 && encoder->residual_enabled);
        break;
    case 16:
        RESIDUAL_REQUIRE(sixel_encoder_set_6delta_plane_size(encoder, 32, 16)
            == SIXEL_OK);
        RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) != SIXEL_OK);
        RESIDUAL_REQUIRE(out.size == 0);
        break;
    case 17:
        sixel_progressive_config_init(&config, sizeof(config));
        RESIDUAL_REQUIRE(sixel_progressive_new(&progressive, encoder,
            &config, NULL) != SIXEL_OK && progressive == NULL);
        break;
    case 18:
        RESIDUAL_REQUIRE(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_UPDATE_POLICY, "delta:R0:Drgb241:T8") == SIXEL_OK);
        for (i = 0; i < 4; ++i) {
            RESIDUAL_REQUIRE(residual_test_encode(encoder, &out) == SIXEL_OK);
            RESIDUAL_REQUIRE(sixel_encoder_get_6delta_painted_pixels(encoder)
                == 0 && encoder->residual_state.age[0] == 0);
        }
        break;
    default:
        goto end;
    }
    ok = 1;
end:
    sixel_progressive_unref(progressive);
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}
