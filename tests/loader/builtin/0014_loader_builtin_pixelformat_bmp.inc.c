/*
 * BMP numeric cases are split out from 0014 main body to keep dispatch and
 * case maintenance localized while preserving original env entry keys.
 */

static SIXELSTATUS
capture_bmp_numeric_probe(sixel_frame_t *frame, void *data)
{
    bmp_numeric_probe_context_t *context;
    size_t sample_count;
    size_t sample_limit;

    context = (bmp_numeric_probe_context_t *)data;
    sample_count = 0u;
    sample_limit = 0u;
    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->colorspace = sixel_frame_get_colorspace(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);
    context->transparent = sixel_frame_get_transparent(frame);
    context->alpha_zero_is_transparent = frame->alpha_zero_is_transparent;
    context->has_transparent_mask = frame->transparent_mask != NULL ? 1 : 0;
    context->transparent_mask_size = frame->transparent_mask_size;
    memset(context->pixels_f32, 0, sizeof(context->pixels_f32));
    memset(context->pixels_u8, 0, sizeof(context->pixels_u8));
    memset(context->transparent_mask, 0, sizeof(context->transparent_mask));

    if (context->width <= 0 || context->height <= 0) {
        return SIXEL_OK;
    }
    if ((size_t)context->width > SIZE_MAX / (size_t)context->height) {
        return SIXEL_OK;
    }
    sample_count = (size_t)context->width * (size_t)context->height * 3u;
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(context->pixelformat)) {
        if (frame->pixels.f32ptr == NULL) {
            return SIXEL_OK;
        }
        sample_limit = sizeof(context->pixels_f32)
            / sizeof(context->pixels_f32[0]);
        if (sample_count > sample_limit) {
            sample_count = sample_limit;
        }
        memcpy(context->pixels_f32,
               frame->pixels.f32ptr,
               sample_count * sizeof(context->pixels_f32[0]));
    } else if (frame->pixels.u8ptr != NULL) {
        sample_limit = sizeof(context->pixels_u8)
            / sizeof(context->pixels_u8[0]);
        if (sample_count > sample_limit) {
            sample_count = sample_limit;
        }
        memcpy(context->pixels_u8, frame->pixels.u8ptr, sample_count);
    }
    if (frame->transparent_mask != NULL && frame->transparent_mask_size >= 4u) {
        memcpy(context->transparent_mask,
               frame->transparent_mask,
               sizeof(context->transparent_mask));
    }

    return SIXEL_OK;
}

static float
bmp_numeric_decode_srgb_unit(float gamma_value)
{
    if (!(gamma_value > 0.0f)) {
        return 0.0f;
    }
    if (gamma_value >= 1.0f) {
        return 1.0f;
    }
    if (gamma_value <= 0.04045f) {
        return gamma_value / 12.92f;
    }
    return powf((gamma_value + 0.055f) / 1.055f, 2.4f);
}

static float
bmp_numeric_encode_srgb_unit(float linear_value)
{
    if (!(linear_value > 0.0f)) {
        return 0.0f;
    }
    if (linear_value >= 1.0f) {
        return 1.0f;
    }
    if (linear_value <= 0.0031308f) {
        return linear_value * 12.92f;
    }
    return 1.055f * powf(linear_value, 1.0f / 2.4f) - 0.055f;
}

static unsigned char
bmp_numeric_linear_to_u8(float linear_value)
{
    float gamma_value;
    int scaled;

    gamma_value = bmp_numeric_encode_srgb_unit(linear_value);
    scaled = (int)floorf(gamma_value * 255.0f + 0.5f);
    if (scaled < 0) {
        scaled = 0;
    } else if (scaled > 255) {
        scaled = 255;
    }
    return (unsigned char)scaled;
}

static void
bmp_numeric_linear_samples_to_rgb888(unsigned char out_rgb[12],
                                     float const in_linear[12])
{
    size_t index;

    index = 0u;
    if (out_rgb == NULL || in_linear == NULL) {
        return;
    }
    for (index = 0u; index < 12u; ++index) {
        out_rgb[index] = bmp_numeric_linear_to_u8(in_linear[index]);
    }
}

static void
bmp_numeric_compose_expected_linear(float out_rgb[12],
                                    unsigned char const rgba[16],
                                    float const bg_linear[3])
{
    size_t index;
    int channel;
    float alpha_unit;
    float inv_alpha;
    float src_gamma;
    float src_linear;

    index = 0u;
    channel = 0;
    alpha_unit = 0.0f;
    inv_alpha = 0.0f;
    src_gamma = 0.0f;
    src_linear = 0.0f;
    if (out_rgb == NULL || rgba == NULL || bg_linear == NULL) {
        return;
    }
    for (index = 0u; index < 4u; ++index) {
        alpha_unit = (float)rgba[index * 4u + 3u] / 255.0f;
        inv_alpha = 1.0f - alpha_unit;
        for (channel = 0; channel < 3; ++channel) {
            src_gamma = (float)rgba[index * 4u + (size_t)channel] / 255.0f;
            src_linear = bmp_numeric_decode_srgb_unit(src_gamma);
            out_rgb[index * 3u + (size_t)channel] =
                src_linear * alpha_unit
                + bg_linear[channel] * inv_alpha;
        }
    }
}

static void
bmp_numeric_compose_expected_linear_u16(float out_rgb[12],
                                        uint16_t const rgba16[16],
                                        float const bg_linear[3])
{
    size_t index;
    int channel;
    float alpha_unit;
    float inv_alpha;
    float src_gamma;
    float src_linear;

    index = 0u;
    channel = 0;
    alpha_unit = 0.0f;
    inv_alpha = 0.0f;
    src_gamma = 0.0f;
    src_linear = 0.0f;
    if (out_rgb == NULL || rgba16 == NULL || bg_linear == NULL) {
        return;
    }
    for (index = 0u; index < 4u; ++index) {
        alpha_unit = (float)rgba16[index * 4u + 3u] / 65535.0f;
        inv_alpha = 1.0f - alpha_unit;
        for (channel = 0; channel < 3; ++channel) {
            src_gamma = (float)rgba16[index * 4u + (size_t)channel]
                / 65535.0f;
            src_linear = bmp_numeric_decode_srgb_unit(src_gamma);
            out_rgb[index * 3u + (size_t)channel] =
                src_linear * alpha_unit
                + bg_linear[channel] * inv_alpha;
        }
    }
}

static int
verify_bmp_float_probe(char const *label,
                       bmp_numeric_probe_context_t const *probe,
                       float const expected_rgb[12],
                       float tolerance)
{
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    unsigned char expected_rgb_u8[12];
    size_t index;

    memset(expected_rgb_u8, 0, sizeof(expected_rgb_u8));
    index = 0u;
    if (label == NULL || probe == NULL || expected_rgb == NULL) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (!(probe->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
          probe->pixelformat == SIXEL_PIXELFORMAT_RGB888)) {
        fprintf(stderr, "%s: pixelformat mismatch (%d)\n",
                label,
                probe->pixelformat);
        return 1;
    }
    if ((probe->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
         probe->colorspace != SIXEL_COLORSPACE_LINEAR) ||
        (probe->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
         probe->colorspace != SIXEL_COLORSPACE_GAMMA)) {
        fprintf(stderr, "%s: colorspace mismatch (%d)\n",
                label,
                probe->colorspace);
        return 1;
    }
    if (probe->width != 2 || probe->height != 2) {
        fprintf(stderr, "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }
    if (probe->alpha_zero_is_transparent != 1) {
        fprintf(stderr, "%s: alpha_zero_is_transparent mismatch (%d)\n",
                label,
                probe->alpha_zero_is_transparent);
        return 1;
    }
    if (probe->has_transparent_mask != 1 ||
        probe->transparent_mask_size < sizeof(expected_mask)) {
        fprintf(stderr, "%s: transparent mask mismatch (%d, %zu)\n",
                label,
                probe->has_transparent_mask,
                probe->transparent_mask_size);
        return 1;
    }
    if (memcmp(probe->transparent_mask,
               expected_mask,
               sizeof(expected_mask)) != 0) {
        fprintf(stderr, "%s: transparent mask samples mismatch\n", label);
        return 1;
    }
    if (probe->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        for (index = 0u; index < 12u; ++index) {
            if (!float_approx_equal(probe->pixels_f32[index],
                                    expected_rgb[index],
                                    tolerance)) {
                fprintf(stderr,
                        "%s: sample %zu mismatch (actual=%0.8f expected=%0.8f)\n",
                        label,
                        index,
                        probe->pixels_f32[index],
                        expected_rgb[index]);
                return 1;
            }
        }
    } else {
        bmp_numeric_linear_samples_to_rgb888(expected_rgb_u8, expected_rgb);
        if (memcmp(probe->pixels_u8,
                   expected_rgb_u8,
                   sizeof(expected_rgb_u8)) != 0) {
            fprintf(stderr, "%s: RGB samples mismatch\n", label);
            return 1;
        }
    }

    return 0;
}

static int
verify_bmp_rgb_probe(char const *label,
                     bmp_numeric_probe_context_t const *probe,
                     int expected_width,
                     int expected_height,
                     unsigned char const *expected_rgb,
                     size_t expected_rgb_size)
{
    if (label == NULL ||
        probe == NULL ||
        expected_rgb == NULL ||
        expected_rgb_size == 0u) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (probe->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr, "%s: pixelformat mismatch (%d)\n",
                label,
                probe->pixelformat);
        return 1;
    }
    if (probe->colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr, "%s: colorspace mismatch (%d)\n",
                label,
                probe->colorspace);
        return 1;
    }
    if (probe->width != expected_width || probe->height != expected_height) {
        fprintf(stderr, "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }
    if (probe->alpha_zero_is_transparent != 0) {
        fprintf(stderr, "%s: alpha_zero_is_transparent mismatch (%d)\n",
                label,
                probe->alpha_zero_is_transparent);
        return 1;
    }
    if (probe->has_transparent_mask != 0 ||
        probe->transparent_mask_size != 0u) {
        fprintf(stderr, "%s: unexpected transparent mask (%d, %zu)\n",
                label,
                probe->has_transparent_mask,
                probe->transparent_mask_size);
        return 1;
    }
    if (memcmp(probe->pixels_u8, expected_rgb, expected_rgb_size) != 0) {
        fprintf(stderr, "%s: RGB samples mismatch\n", label);
        return 1;
    }
    return 0;
}

static int
verify_bmp_png16_no_bg_rgbf_probe(char const *label,
                                  bmp_numeric_probe_context_t const *probe,
                                  int expect_mask)
{
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };

    if (label == NULL || probe == NULL) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (!((probe->pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32 &&
           probe->colorspace == SIXEL_COLORSPACE_GAMMA) ||
          (probe->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
           probe->colorspace == SIXEL_COLORSPACE_LINEAR) ||
          (probe->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
           probe->colorspace == SIXEL_COLORSPACE_GAMMA))) {
        fprintf(stderr, "%s: format/colorspace mismatch (%d,%d)\n",
                label,
                probe->pixelformat,
                probe->colorspace);
        return 1;
    }
    if (probe->width != 2 || probe->height != 2) {
        fprintf(stderr, "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }
    if (expect_mask) {
        if (probe->alpha_zero_is_transparent != 1 ||
            probe->has_transparent_mask != 1 ||
            probe->transparent_mask_size < sizeof(expected_mask)) {
            fprintf(stderr, "%s: transparent-mask metadata mismatch\n",
                    label);
            return 1;
        }
        if (memcmp(probe->transparent_mask,
                   expected_mask,
                   sizeof(expected_mask)) != 0) {
            fprintf(stderr, "%s: transparent-mask samples mismatch\n",
                    label);
            return 1;
        }
    } else {
        if (probe->alpha_zero_is_transparent != 0 ||
            probe->has_transparent_mask != 0 ||
            probe->transparent_mask_size != 0u) {
            fprintf(stderr, "%s: unexpected transparency metadata\n",
                    label);
            return 1;
        }
    }

    return 0;
}

static int
verify_bmp_rgb_mask_probe(char const *label,
                          bmp_numeric_probe_context_t const *probe,
                          int expected_width,
                          int expected_height,
                          unsigned char const *expected_rgb,
                          size_t expected_rgb_size,
                          unsigned char const *expected_mask,
                          size_t expected_mask_size)
{
    if (label == NULL ||
        probe == NULL ||
        expected_rgb == NULL ||
        expected_mask == NULL ||
        expected_rgb_size == 0u ||
        expected_mask_size == 0u) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (probe->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr, "%s: pixelformat mismatch (%d)\n",
                label,
                probe->pixelformat);
        return 1;
    }
    if (probe->colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr, "%s: colorspace mismatch (%d)\n",
                label,
                probe->colorspace);
        return 1;
    }
    if (probe->width != expected_width || probe->height != expected_height) {
        fprintf(stderr, "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }
    if (probe->alpha_zero_is_transparent != 1) {
        fprintf(stderr, "%s: alpha_zero_is_transparent mismatch (%d)\n",
                label,
                probe->alpha_zero_is_transparent);
        return 1;
    }
    if (probe->has_transparent_mask != 1 ||
        probe->transparent_mask_size < expected_mask_size) {
        fprintf(stderr, "%s: transparent mask mismatch (%d, %zu)\n",
                label,
                probe->has_transparent_mask,
                probe->transparent_mask_size);
        return 1;
    }
    if (memcmp(probe->pixels_u8, expected_rgb, expected_rgb_size) != 0) {
        fprintf(stderr, "%s: RGB samples mismatch\n", label);
        return 1;
    }
    if (memcmp(probe->transparent_mask,
               expected_mask,
               expected_mask_size) != 0) {
        fprintf(stderr, "%s: transparent mask samples mismatch\n", label);
        return 1;
    }
    return 0;
}

static int
run_builtin_loader_bmp_rgba_bgcolor_float32_numeric_test(void)
{
    static unsigned char const bmp_rgba_2x2_sample[] = {
        0x42u, 0x4du, 0x46u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u,
        0x28u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x02u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x20u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u,
        0x13u, 0x0bu, 0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u, 0x40u, 0xffu, 0xffu, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0x00u, 0xffu, 0x00u, 0x80u
    };
    static unsigned char const src_rgba_topdown[16] = {
        0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x80u,
        0x00u, 0x00u, 0xffu, 0x40u, 0xffu, 0xffu, 0xffu, 0x00u
    };
    static unsigned char const bgcolor_unit_u8[3] = { 0x40u, 0x80u, 0xc0u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe_gamma;
    bmp_numeric_probe_context_t probe_linear;
    SIXELSTATUS status;
    float expected_gamma[12];
    float expected_linear[12];
    float bg_gamma_linear[3];
    float bg_linear[3];
    int result;
    int channel;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe_gamma, 0, sizeof(probe_gamma));
    memset(&probe_linear, 0, sizeof(probe_linear));
    memset(expected_gamma, 0, sizeof(expected_gamma));
    memset(expected_linear, 0, sizeof(expected_linear));
    memset(bg_gamma_linear, 0, sizeof(bg_gamma_linear));
    memset(bg_linear, 0, sizeof(bg_linear));
    result = 1;
    channel = 0;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_unit_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_GAMMA);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader bmp rgba bgcolor float32 numeric (gamma bg)",
        bmp_rgba_2x2_sample,
        sizeof(bmp_rgba_2x2_sample),
        &options,
        capture_bmp_numeric_probe,
        &probe_gamma,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp rgba bgcolor float32 numeric: "
                "loader failed for gamma background (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        bg_gamma_linear[channel] = bmp_numeric_decode_srgb_unit(
            (float)bgcolor_unit_u8[channel] / 255.0f);
    }
    bmp_numeric_compose_expected_linear(expected_gamma,
                                        src_rgba_topdown,
                                        bg_gamma_linear);
    result = verify_bmp_float_probe(
        "builtin loader bmp rgba bgcolor float32 numeric (gamma bg)",
        &probe_gamma,
        expected_gamma,
        0.00001f);
    if (result != 0) {
        goto end;
    }

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader bmp rgba bgcolor float32 numeric (linear bg)",
        bmp_rgba_2x2_sample,
        sizeof(bmp_rgba_2x2_sample),
        &options,
        capture_bmp_numeric_probe,
        &probe_linear,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp rgba bgcolor float32 numeric: "
                "loader failed for linear background (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = (float)bgcolor_unit_u8[channel] / 255.0f;
    }
    bmp_numeric_compose_expected_linear(expected_linear,
                                        src_rgba_topdown,
                                        bg_linear);
    result = verify_bmp_float_probe(
        "builtin loader bmp rgba bgcolor float32 numeric (linear bg)",
        &probe_linear,
        expected_linear,
        0.00001f);
    if (result != 0) {
        goto end;
    }

    if (probe_gamma.pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
        probe_linear.pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        if (float_approx_equal(probe_gamma.pixels_f32[3],
                               probe_linear.pixels_f32[3],
                               0.000001f)) {
            fprintf(stderr,
                    "builtin loader bmp rgba bgcolor float32 numeric: "
                    "background colorspace did not affect output\n");
            result = 1;
            goto end;
        }
    } else if (probe_gamma.pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
               probe_linear.pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        if (memcmp(probe_gamma.pixels_u8, probe_linear.pixels_u8, 12u) == 0) {
            fprintf(stderr,
                    "builtin loader bmp rgba bgcolor float32 numeric: "
                    "background colorspace did not affect output\n");
            result = 1;
            goto end;
        }
    }
    result = 0;

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_builtin_loader_bmp_rgba_mask_no_bg_numeric_test(void)
{
    static unsigned char const bmp_rgba_2x2_sample[] = {
        0x42u, 0x4du, 0x46u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u,
        0x28u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x02u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x20u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u,
        0x13u, 0x0bu, 0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u, 0x40u, 0xffu, 0xffu, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0x00u, 0xffu, 0x00u, 0x80u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader bmp rgba mask without bgcolor numeric",
        bmp_rgba_2x2_sample,
        sizeof(bmp_rgba_2x2_sample),
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "colorspace mismatch (%d)\n",
                probe.colorspace);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 1) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "alpha_zero_is_transparent mismatch (%d)\n",
                probe.alpha_zero_is_transparent);
        return 1;
    }
    if (probe.has_transparent_mask != 1 || probe.transparent_mask_size < 4u) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "transparent mask missing (%d, %zu)\n",
                probe.has_transparent_mask,
                probe.transparent_mask_size);
        return 1;
    }
    if (memcmp(probe.transparent_mask,
               expected_mask,
               sizeof(expected_mask)) != 0) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "transparent mask mismatch\n");
        return 1;
    }
    if (memcmp(probe.pixels_u8, expected_rgb, sizeof(expected_rgb)) != 0) {
        fprintf(stderr,
                "builtin loader bmp rgba mask without bgcolor numeric: "
                "RGB composite mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_opaque_fastpath_numeric_test(void)
{
    static unsigned char const bmp_rgb_2x1_sample[] = {
        0x42u, 0x4du, 0x3eu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u,
        0x28u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x18u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x08u, 0x00u, 0x00u, 0x00u,
        0x13u, 0x0bu, 0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const expected_rgb[6] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader bmp opaque fastpath numeric",
        bmp_rgb_2x1_sample,
        sizeof(bmp_rgb_2x1_sample),
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp opaque fastpath numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader bmp opaque fastpath numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader bmp opaque fastpath numeric: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader bmp opaque fastpath numeric: "
                "colorspace mismatch (%d)\n",
                probe.colorspace);
        return 1;
    }
    if (probe.width != 2 || probe.height != 1) {
        fprintf(stderr,
                "builtin loader bmp opaque fastpath numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr,
                "builtin loader bmp opaque fastpath numeric: "
                "alpha_zero_is_transparent mismatch (%d)\n",
                probe.alpha_zero_is_transparent);
        return 1;
    }
    if (probe.has_transparent_mask != 0 || probe.transparent_mask_size != 0u) {
        fprintf(stderr,
                "builtin loader bmp opaque fastpath numeric: "
                "unexpected transparent mask (%d, %zu)\n",
                probe.has_transparent_mask,
                probe.transparent_mask_size);
        return 1;
    }
    if (memcmp(probe.pixels_u8, expected_rgb, sizeof(expected_rgb)) != 0) {
        fprintf(stderr,
                "builtin loader bmp opaque fastpath numeric: "
                "RGB samples mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_rle8_decode_numeric_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp rle8 decode numeric",
        "/tests/data/inputs/formats/snake-bmp3-rle8.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp rle8 decode numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader bmp rle8 decode numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader bmp rle8 decode numeric: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader bmp rle8 decode numeric: "
                "colorspace mismatch (%d)\n",
                probe.colorspace);
        return 1;
    }
    if (probe.width <= 0 || probe.height <= 0) {
        fprintf(stderr,
                "builtin loader bmp rle8 decode numeric: "
                "invalid geometry (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr,
                "builtin loader bmp rle8 decode numeric: "
                "alpha_zero_is_transparent mismatch (%d)\n",
                probe.alpha_zero_is_transparent);
        return 1;
    }
    if (probe.has_transparent_mask != 0 ||
        probe.transparent_mask_size != 0u) {
        fprintf(stderr,
                "builtin loader bmp rle8 decode numeric: "
                "unexpected transparent mask (%d, %zu)\n",
                probe.has_transparent_mask,
                probe.transparent_mask_size);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_rle4_decode_numeric_test(void)
{
    static unsigned char const bmp_rle4_2x2_sample[] = {
        0x42u, 0x4du, 0x52u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x46u, 0x00u, 0x00u, 0x00u,
        0x28u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x02u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x04u, 0x00u,
        0x02u, 0x00u, 0x00u, 0x00u, 0x0cu, 0x00u, 0x00u, 0x00u,
        0x13u, 0x0bu, 0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u,
        0x04u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u,
        0x02u, 0x30u, 0x00u, 0x00u, 0x02u, 0x12u,
        0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader bmp rle4 decode numeric",
        bmp_rle4_2x2_sample,
        sizeof(bmp_rle4_2x2_sample),
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp rle4 decode numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader bmp rle4 decode numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "builtin loader bmp rle4 decode numeric: "
                "pixelformat mismatch (%d)\n",
                probe.pixelformat);
        return 1;
    }
    if (probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader bmp rle4 decode numeric: "
                "colorspace mismatch (%d)\n",
                probe.colorspace);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2) {
        fprintf(stderr,
                "builtin loader bmp rle4 decode numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr,
                "builtin loader bmp rle4 decode numeric: "
                "alpha_zero_is_transparent mismatch (%d)\n",
                probe.alpha_zero_is_transparent);
        return 1;
    }
    if (probe.has_transparent_mask != 0 ||
        probe.transparent_mask_size != 0u) {
        fprintf(stderr,
                "builtin loader bmp rle4 decode numeric: "
                "unexpected transparent mask (%d, %zu)\n",
                probe.has_transparent_mask,
                probe.transparent_mask_size);
        return 1;
    }
    if (memcmp(probe.pixels_u8, expected_rgb, sizeof(expected_rgb)) != 0) {
        fprintf(stderr,
                "builtin loader bmp rle4 decode numeric: "
                "RGB samples mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_rle_broken_fail_numeric_test(void)
{
    static unsigned char const bmp_rle8_broken_sample[] = {
        0x42u, 0x4du, 0x41u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x3eu, 0x00u, 0x00u, 0x00u,
        0x28u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x02u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x08u, 0x00u,
        0x01u, 0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u,
        0x13u, 0x0bu, 0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u,
        0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0x00u,
        0x00u, 0x02u, 0x01u
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(
        "builtin loader bmp rle broken fail numeric",
        bmp_rle8_broken_sample,
        sizeof(bmp_rle8_broken_sample),
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader bmp rle broken fail numeric: "
                "unexpected success\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_core12_1bpp_palette_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp core12 1bpp palette numeric",
        "/tests/data/inputs/formats/bmp-core12-1bpp-pal-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp core12 1bpp palette numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    return verify_bmp_rgb_probe(
        "builtin loader bmp core12 1bpp palette numeric",
        &probe,
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_info40_4bpp_palette_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp info40 4bpp palette numeric",
        "/tests/data/inputs/formats/bmp-info40-4bpp-pal-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp info40 4bpp palette numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    return verify_bmp_rgb_probe(
        "builtin loader bmp info40 4bpp palette numeric",
        &probe,
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_bitfields_rgb565_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bitfields rgb565 numeric",
        "/tests/data/inputs/formats/bmp-info40-bitfields-rgb565-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bitfields rgb565 numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    return verify_bmp_rgb_probe(
        "builtin loader bmp bitfields rgb565 numeric",
        &probe,
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_bitfields_alpha_mask_no_bg_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bitfields alpha mask without bgcolor numeric",
        "/tests/data/inputs/formats/bmp-info40-bitfields-rgba-mask-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bitfields alpha mask without bgcolor "
                "numeric: loader failed (%d)\n",
                (int)status);
        return 1;
    }
    return verify_bmp_rgb_mask_probe(
        "builtin loader bmp bitfields alpha mask without bgcolor numeric",
        &probe,
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb),
        expected_mask,
        sizeof(expected_mask));
}

static int
run_builtin_loader_bmp_v4_alpha_bgcolor_float32_numeric_test(void)
{
    static unsigned char const src_rgba_topdown[16] = {
        0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x80u,
        0x00u, 0x00u, 0xffu, 0x40u, 0xffu, 0xffu, 0xffu, 0x00u
    };
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    float expected_linear[12];
    float bg_linear[3];
    int result;
    int channel;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(expected_linear, 0, sizeof(expected_linear));
    memset(bg_linear, 0, sizeof(bg_linear));
    result = 1;
    channel = 0;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp v4 alpha bgcolor float32 numeric",
        "/tests/data/inputs/formats/bmp-v4-bitfields-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v4 alpha bgcolor float32 numeric: "
                "loader failed (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = (float)bgcolor_u8[channel] / 255.0f;
    }
    bmp_numeric_compose_expected_linear(expected_linear,
                                        src_rgba_topdown,
                                        bg_linear);
    result = verify_bmp_float_probe(
        "builtin loader bmp v4 alpha bgcolor float32 numeric",
        &probe,
        expected_linear,
        0.00001f);

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_builtin_loader_bmp_topdown_24bpp_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp topdown 24bpp numeric",
        "/tests/data/inputs/formats/bmp-info40-topdown-24bpp-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp topdown 24bpp numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    return verify_bmp_rgb_probe(
        "builtin loader bmp topdown 24bpp numeric",
        &probe,
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_rle8_absolute_padding_numeric_test(void)
{
    static unsigned char const expected_rgb[9] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u, 0xffu
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp rle8 absolute padding numeric",
        "/tests/data/inputs/formats/bmp-info40-rle8-absolute-padding-3x1.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp rle8 absolute padding numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    return verify_bmp_rgb_probe(
        "builtin loader bmp rle8 absolute padding numeric",
        &probe,
        3,
        1,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_fail_rle8_delta_topdown_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail rle8 delta topdown numeric",
        "/tests/data/inputs/formats/bmp-info40-rle8-delta-2x2-topdown.bmp");
}

static int
run_builtin_loader_bmp_rle4_mixed_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp rle4 mixed numeric",
        "/tests/data/inputs/formats/bmp-info40-rle4-mixed-4x1.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp rle4 mixed numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    return verify_bmp_rgb_probe(
        "builtin loader bmp rle4 mixed numeric",
        &probe,
        4,
        1,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_rle4_invalid_delta_fail_numeric_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp rle4 invalid delta fail numeric",
        "/tests/data/inputs/formats/bmp-info40-rle4-invalid-delta-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader bmp rle4 invalid delta fail numeric: "
                "unexpected success\n");
        return 1;
    }
    if (probe.callback_count != 0) {
        fprintf(stderr,
                "builtin loader bmp rle4 invalid delta fail numeric: "
                "unexpected callback (%d)\n",
                probe.callback_count);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_rgb_fixture_case(
    char const *label,
    char const *fixture_path,
    int expected_width,
    int expected_height,
    unsigned char const *expected_rgb,
    size_t expected_rgb_size)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(label,
                                           fixture_path,
                                           &options,
                                           capture_bmp_numeric_probe,
                                           &probe,
                                           &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: loader failed (%d)\n", label, (int)status);
        return 1;
    }
    return verify_bmp_rgb_probe(label,
                                &probe,
                                expected_width,
                                expected_height,
                                expected_rgb,
                                expected_rgb_size);
}

static int
run_builtin_loader_bmp_expect_fail_case(char const *label,
                                        char const *fixture_path)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_case(label,
                                           fixture_path,
                                           &options,
                                           capture_bmp_numeric_probe,
                                           &probe,
                                           &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "%s: unexpected success\n", label);
        return 1;
    }
    if (probe.callback_count != 0) {
        fprintf(stderr, "%s: unexpected callback (%d)\n",
                label,
                probe.callback_count);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_rgb_buffer_case(char const *label,
                                       unsigned char const *buffer,
                                       size_t buffer_size,
                                       int expected_width,
                                       int expected_height,
                                       unsigned char const *expected_rgb,
                                       size_t expected_rgb_size)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(label,
                                                  buffer,
                                                  buffer_size,
                                                  &options,
                                                  capture_bmp_numeric_probe,
                                                  &probe,
                                                  &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: loader failed (%d)\n", label, (int)status);
        return 1;
    }
    return verify_bmp_rgb_probe(label,
                                &probe,
                                expected_width,
                                expected_height,
                                expected_rgb,
                                expected_rgb_size);
}

static int
run_builtin_loader_bmp_expect_fail_buffer_case(char const *label,
                                                unsigned char const *buffer,
                                                size_t buffer_size)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_buffer_case(label,
                                                  buffer,
                                                  buffer_size,
                                                  &options,
                                                  capture_bmp_numeric_probe,
                                                  &probe,
                                                  &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "%s: unexpected success\n", label);
        return 1;
    }
    if (probe.callback_count != 0) {
        fprintf(stderr, "%s: unexpected callback (%d)\n",
                label,
                probe.callback_count);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_probe_fixture_case(char const *label,
                                          char const *fixture_path,
                                          sixel_frombmp_probe_t *probe_out,
                                          SIXELSTATUS *status_out)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    char const *source_root;
#if defined(_MSC_VER)
    char *source_root_dupe;
    size_t source_root_len;
#endif
    char image_path[PATH_MAX];
    int cancel_flag;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    source_root = NULL;
    cancel_flag = 0;
    result = 1;
#if defined(_MSC_VER)
    source_root_dupe = NULL;
    source_root_len = 0u;
    _dupenv_s(&source_root_dupe, &source_root_len, "MESON_SOURCE_ROOT");
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "abs_top_srcdir");
    }
    if (source_root_dupe == NULL) {
        _dupenv_s(&source_root_dupe, &source_root_len, "TOP_SRCDIR");
    }
    if (source_root_dupe != NULL) {
        source_root = source_root_dupe;
    }
#else
    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
#endif
    if (source_root == NULL) {
        source_root = ".";
    }

    if (label == NULL || fixture_path == NULL || probe_out == NULL) {
        goto cleanup;
    }
    if (build_image_path(source_root,
                         fixture_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        goto cleanup;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        goto cleanup;
    }
    status = sixel_chunk_new(&chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        goto cleanup;
    }

    status = sixel_frombmp_probe(chunk,
                                 probe_out,
                                 SIXEL_FROMBMP_INFO40_MODE_AUTO);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: frombmp probe failed (%d)\n", label, (int)status);
        goto cleanup;
    }
    result = 0;

cleanup:
    if (status_out != NULL) {
        *status_out = status;
    }
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
#if defined(_MSC_VER)
    if (source_root_dupe != NULL) {
        free(source_root_dupe);
    }
#endif
    return result;
}

static int
run_builtin_loader_bmp_info40_8bpp_palette_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp info40 8bpp palette numeric",
        "/tests/data/inputs/formats/bmp-info40-8bpp-pal-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_info40_topdown_1bpp_palette_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp info40 topdown 1bpp palette numeric",
        "/tests/data/inputs/formats/bmp-info40-topdown-1bpp-pal-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_info40_16bpp_rgb555_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp info40 16bpp rgb555 numeric",
        "/tests/data/inputs/formats/bmp-info40-16bpp-rgb555-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_info40_32bpp_a0_opaque_num_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp info40 32bpp alpha-zero opaque numeric",
        "/tests/data/inputs/formats/bmp-info40-32bpp-alpha-zero-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_v5_alpha_bgcolor_float32_numeric_test(void)
{
    static unsigned char const src_rgba_topdown[16] = {
        0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x80u,
        0x00u, 0x00u, 0xffu, 0x40u, 0xffu, 0xffu, 0xffu, 0x00u
    };
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    float expected_linear[12];
    float bg_linear[3];
    int result;
    int channel;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(expected_linear, 0, sizeof(expected_linear));
    memset(bg_linear, 0, sizeof(bg_linear));
    result = 1;
    channel = 0;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp v5 alpha bgcolor float32 numeric",
        "/tests/data/inputs/formats/bmp-v5-bitfields-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v5 alpha bgcolor float32 numeric: "
                "loader failed (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = (float)bgcolor_u8[channel] / 255.0f;
    }
    bmp_numeric_compose_expected_linear(expected_linear,
                                        src_rgba_topdown,
                                        bg_linear);
    result = verify_bmp_float_probe(
        "builtin loader bmp v5 alpha bgcolor float32 numeric",
        &probe,
        expected_linear,
        0.00001f);

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_builtin_loader_bmp_fail_unsupported_dib_size_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail unsupported dib size numeric",
        "/tests/data/inputs/formats/bmp-bad-dibsize-2x2.bmp");
}

static int
run_builtin_loader_bmp_fail_rle8_requires_8bpp_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail rle8 requires 8bpp numeric",
        "/tests/data/inputs/formats/bmp-bad-rle8-requires-8bpp.bmp");
}

static int
run_builtin_loader_bmp_fail_rle4_requires_4bpp_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail rle4 requires 4bpp numeric",
        "/tests/data/inputs/formats/bmp-bad-rle4-requires-4bpp.bmp");
}

static int
run_builtin_loader_bmp_fail_invalid_color_masks_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail invalid color masks numeric",
        "/tests/data/inputs/formats/bmp-bad-bitfields-zero-masks.bmp");
}

static int
run_builtin_loader_bmp_fail_rle8_no_eom_num_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail rle8 missing end marker numeric",
        "/tests/data/inputs/formats/bmp-bad-rle8-missing-eob.bmp");
}

static int
run_builtin_loader_bmp_info40_8bpp_palette_used_num_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0xffu, 0x00u, 0x00u
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp info40 8bpp palette colors-used numeric",
        "/tests/data/inputs/formats/"
        "bmp-info40-8bpp-pal-colors-used-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_info40_32bpp_bf_no_alpha_num_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp info40 32bpp bitfields no-alpha numeric",
        "/tests/data/inputs/formats/"
        "bmp-info40-bitfields-rgbx-noalpha-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_v3_alpha_bgcolor_float32_numeric_test(void)
{
    static unsigned char const src_rgba_topdown[16] = {
        0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x80u,
        0x00u, 0x00u, 0xffu, 0x40u, 0xffu, 0xffu, 0xffu, 0x00u
    };
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    float expected_linear[12];
    float bg_linear[3];
    int result;
    int channel;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(expected_linear, 0, sizeof(expected_linear));
    memset(bg_linear, 0, sizeof(bg_linear));
    result = 1;
    channel = 0;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp v3 alpha bgcolor float32 numeric",
        "/tests/data/inputs/formats/bmp-v3-bitfields-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v3 alpha bgcolor float32 numeric: "
                "loader failed (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = (float)bgcolor_u8[channel] / 255.0f;
    }
    bmp_numeric_compose_expected_linear(expected_linear,
                                        src_rgba_topdown,
                                        bg_linear);
    result = verify_bmp_float_probe(
        "builtin loader bmp v3 alpha bgcolor float32 numeric",
        &probe,
        expected_linear,
        0.00001f);

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_builtin_loader_bmp_fail_rle8_topdown_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail rle8 topdown numeric",
        "/tests/data/inputs/formats/bmp-info40-rle8-topdown-2x2.bmp");
}

static int
run_builtin_loader_bmp_fail_rle4_topdown_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail rle4 topdown numeric",
        "/tests/data/inputs/formats/bmp-info40-rle4-topdown-2x2.bmp");
}

static int
run_builtin_loader_bmp_fail_truncated_masks_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail truncated masks numeric",
        "/tests/data/inputs/formats/bmp-bad-bitfields-truncated-masks.bmp");
}

static int
run_builtin_loader_bmp_fail_truncated_pixel_data_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail truncated pixel data numeric",
        "/tests/data/inputs/formats/"
        "bmp-bad-truncated-pixel-data-24bpp-2x2.bmp");
}

static int
run_builtin_loader_bmp_fail_palette_index_overflow_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail palette index overflow numeric",
        "/tests/data/inputs/formats/"
        "bmp-bad-palette-index-overflow-8bpp.bmp");
}

static int
run_builtin_loader_bmp_fail_rle8_absolute_overflow_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail rle8 absolute overflow numeric",
        "/tests/data/inputs/formats/bmp-bad-rle8-absolute-overflow.bmp");
}

static int
run_builtin_loader_bmp_fail_unsupported_comp_num_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail unsupported compression numeric",
        "/tests/data/inputs/formats/bmp-bad-unsupported-compression.bmp");
}

static int
verify_bmp_rgbfloat32_probe_metadata(char const *label,
                                     bmp_numeric_probe_context_t const *probe,
                                     int expected_width,
                                     int expected_height)
{
    if (label == NULL || probe == NULL) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (probe->pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        fprintf(stderr, "%s: pixelformat mismatch (%d)\n",
                label,
                probe->pixelformat);
        return 1;
    }
    if (probe->colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr, "%s: colorspace mismatch (%d)\n",
                label,
                probe->colorspace);
        return 1;
    }
    if (probe->width != expected_width || probe->height != expected_height) {
        fprintf(stderr, "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }
    if (probe->alpha_zero_is_transparent != 0 ||
        probe->has_transparent_mask != 0 ||
        probe->transparent_mask_size != 0u) {
        fprintf(stderr, "%s: unexpected transparent metadata\n", label);
        return 1;
    }
    return 0;
}

static int
run_builtin_loader_bmp_bi_jpeg_cms_off_numeric_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-jpeg cms off numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-jpeg-embedded-esrgb.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-jpeg cms off numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }

    return verify_bmp_rgbfloat32_probe_metadata(
        "builtin loader bmp bi-jpeg cms off numeric",
        &probe,
        64,
        64);
}

static int
run_builtin_loader_bmp_bi_jpeg_cms_on_numeric_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-jpeg cms on numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-jpeg-embedded-esrgb.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-jpeg cms on numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }

    return verify_bmp_rgbfloat32_probe_metadata(
        "builtin loader bmp bi-jpeg cms on numeric",
        &probe,
        64,
        64);
}

static int
run_builtin_loader_bmp_bi_png_alpha_bgcolor_numeric_test(void)
{
    static unsigned char const src_rgba_topdown[16] = {
        0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x80u,
        0x00u, 0x00u, 0xffu, 0x40u, 0xffu, 0xffu, 0xffu, 0x00u
    };
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    float expected_linear[12];
    float bg_linear[3];
    int result;
    int channel;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(expected_linear, 0, sizeof(expected_linear));
    memset(bg_linear, 0, sizeof(bg_linear));
    result = 1;
    channel = 0;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png alpha bgcolor numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png alpha bgcolor numeric: "
                "loader failed (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = (float)bgcolor_u8[channel] / 255.0f;
    }
    bmp_numeric_compose_expected_linear(expected_linear,
                                        src_rgba_topdown,
                                        bg_linear);
    result = verify_bmp_float_probe(
        "builtin loader bmp bi-png alpha bgcolor numeric",
        &probe,
        expected_linear,
        0.00001f);

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_builtin_loader_bmp_bi_png_alpha_mask_no_bg_numeric_test(void)
{
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png alpha mask no-bg numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png alpha mask no-bg numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader bmp bi-png alpha mask no-bg numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader bmp bi-png alpha mask no-bg numeric: "
                "unexpected format/colorspace (%d, %d)\n",
                probe.pixelformat,
                probe.colorspace);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2) {
        fprintf(stderr,
                "builtin loader bmp bi-png alpha mask no-bg numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 1 ||
        probe.has_transparent_mask != 1 ||
        probe.transparent_mask_size < sizeof(expected_mask)) {
        fprintf(stderr,
                "builtin loader bmp bi-png alpha mask no-bg numeric: "
                "transparent-mask metadata mismatch\n");
        return 1;
    }
    if (memcmp(probe.transparent_mask,
               expected_mask,
               sizeof(expected_mask)) != 0) {
        fprintf(stderr,
                "builtin loader bmp bi-png alpha mask no-bg numeric: "
                "transparent-mask samples mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_bi_png16_alpha_bgcolor_numeric_test(void)
{
    static uint16_t const src_rgba16_topdown[16] = {
        0x1234u, 0xabcdu, 0x4000u, 0xffffu,
        0x2222u, 0xeeeeu, 0x0100u, 0x8000u,
        0xffffu, 0x1111u, 0x3333u, 0x4000u,
        0x5555u, 0x9999u, 0xddddu, 0x0000u
    };
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    float expected_linear[12];
    float bg_linear[3];
    int result;
    int channel;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(expected_linear, 0, sizeof(expected_linear));
    memset(bg_linear, 0, sizeof(bg_linear));
    result = 1;
    channel = 0;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png16 alpha bgcolor numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha bgcolor numeric: "
                "loader failed (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }

    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = (float)bgcolor_u8[channel] / 255.0f;
    }
    bmp_numeric_compose_expected_linear_u16(expected_linear,
                                            src_rgba16_topdown,
                                            bg_linear);
    result = verify_bmp_float_probe(
        "builtin loader bmp bi-png16 alpha bgcolor numeric",
        &probe,
        expected_linear,
        0.00002f);

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_builtin_loader_bmp_bi_png16_alpha_mask_no_bg_numeric_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png16 alpha mask no-bg numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha mask no-bg numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    return verify_bmp_png16_no_bg_rgbf_probe(
        "builtin loader bmp bi-png16 alpha mask no-bg numeric",
        &probe,
        1);
}

static int
run_bmp_png16_cms_on_mask_t(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png16 alpha mask no-bg cms on numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha mask no-bg cms on "
                "numeric: loader failed (%d)\n",
                (int)status);
        return 1;
    }

    return verify_bmp_png16_no_bg_rgbf_probe(
        "builtin loader bmp bi-png16 alpha mask no-bg cms on numeric",
        &probe,
        1);
}

static int
run_bmp_png16_cms_on_pref8_t(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "1") != 0) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 cms-on prefer8 numeric: "
                "setenv failed\n");
        return 1;
    }
    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png16 alpha mask no-bg cms on prefer8 "
        "numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "") != 0) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 cms-on prefer8 numeric: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha mask no-bg cms on "
                "prefer8 numeric: loader failed (%d)\n",
                (int)status);
        return 1;
    }

    return verify_bmp_png16_no_bg_rgbf_probe(
        "builtin loader bmp bi-png16 alpha mask no-bg cms on prefer8 "
        "numeric",
        &probe,
        1);
}

static int
run_bmp_png16_cms_on_opaque_t(void)
{
    static unsigned char const payload[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 0x49u, 0x48u, 0x44u, 0x52u,
        0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u,
        0x10u, 0x02u, 0x00u, 0x00u, 0x00u, 0xadu, 0x44u, 0x46u,
        0x30u, 0x00u, 0x00u, 0x00u, 0x1au, 0x49u, 0x44u, 0x41u,
        0x54u, 0x08u, 0xd7u, 0x25u, 0xc6u, 0xb1u, 0x01u, 0x00u,
        0x00u, 0x08u, 0xc3u, 0x20u, 0xfcu, 0xffu, 0xe8u, 0x74u,
        0x90u, 0x09u, 0x05u, 0x14u, 0xf7u, 0xa9u, 0x62u, 0x72u,
        0xb4u, 0x09u, 0xf8u, 0xd5u, 0xf8u, 0x24u, 0xa4u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x49u, 0x45u, 0x4eu, 0x44u, 0xaeu,
        0x42u, 0x60u, 0x82u
    };
    unsigned char bmp[256];
    size_t bmp_size;
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    bmp_size = 0u;
    status = SIXEL_FALSE;
    memset(bmp, 0, sizeof(bmp));
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;
    if (!bmp_num_mk_bi_png40(bmp,
                             sizeof(bmp),
                             32u,
                             payload,
                             sizeof(payload),
                             &bmp_size)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 opaque cms on numeric: "
                "failed to build bmp buffer\n");
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    result = run_builtin_loader_probe_buffer_case(
        "builtin loader bmp bi-png16 opaque no-bg cms on numeric",
        bmp,
        bmp_size,
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 opaque no-bg cms on "
                "numeric: loader failed (%d)\n",
                (int)status);
        return 1;
    }

    return verify_bmp_png16_no_bg_rgbf_probe(
        "builtin loader bmp bi-png16 opaque no-bg cms on numeric",
        &probe,
        0);
}

static int
run_bmp_png16_bg_cms_on_t(void)
{
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png16 alpha bgcolor cms on numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha bgcolor cms on "
                "numeric: loader failed (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }
    result = verify_bmp_float_probe_metadata(
        "builtin loader bmp bi-png16 alpha bgcolor cms on numeric",
        &probe,
        2,
        2,
        1);

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_bmp_png16_icc_cms_on_num_t(void)
{
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options_off;
    builtin_loader_probe_options_t options_on;
    bmp_numeric_probe_context_t probe_off;
    bmp_numeric_probe_context_t probe_on;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options_off, 0, sizeof(options_off));
    memset(&options_on, 0, sizeof(options_on));
    memset(&probe_off, 0, sizeof(probe_off));
    memset(&probe_on, 0, sizeof(probe_on));
    result = 1;

    options_off.require_static = 1;
    options_off.use_palette = 0;
    options_off.reqcolors = 256;
    options_off.set_bgcolor = 0;
    options_off.bgcolor = NULL;
    options_off.set_loop_control = 0;
    options_off.loop_control = SIXEL_LOOP_AUTO;
    options_off.set_cms_engine = 0;
    options_off.cms_engine = SIXEL_CMS_ENGINE_NONE;
    options_on = options_off;
    options_on.set_cms_engine = 1;
    options_on.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png16 alpha no-bg icc cms off baseline",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-icc-2x2.bmp",
        &options_off,
        capture_bmp_numeric_probe,
        &probe_off,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha no-bg icc cms off "
                "baseline: loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (verify_bmp_png16_no_bg_rgbf_probe(
            "builtin loader bmp bi-png16 alpha no-bg icc cms off baseline",
            &probe_off,
            1) != 0) {
        return 1;
    }
    if (memcmp(probe_off.transparent_mask,
               expected_mask,
               sizeof(expected_mask)) != 0) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha no-bg icc cms off "
                "baseline: transparent-mask mismatch\n");
        return 1;
    }

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png16 alpha no-bg icc cms on numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-icc-2x2.bmp",
        &options_on,
        capture_bmp_numeric_probe,
        &probe_on,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha no-bg icc cms on "
                "numeric: loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (verify_bmp_png16_no_bg_rgbf_probe(
            "builtin loader bmp bi-png16 alpha no-bg icc cms on numeric",
            &probe_on,
            1) != 0) {
        return 1;
    }
    if (memcmp(probe_on.transparent_mask,
               expected_mask,
               sizeof(expected_mask)) != 0) {
        fprintf(stderr,
                "builtin loader bmp bi-png16 alpha no-bg icc cms on "
                "numeric: transparent-mask mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_bi_png_opaque_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp bi-png opaque numeric",
        "/tests/data/inputs/formats/bmp-info40-bi-png-rgb-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_bi_png_linked_icc_num_test(void)
{
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp bi-png linked outer inner icc numeric",
        "/tests/data/inputs/formats/bmp-v5-bi-png-linked-inner-icc.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp bi-png linked outer inner icc numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }

    return verify_bmp_float_probe_metadata(
        "builtin loader bmp bi-png linked outer inner icc numeric",
        &probe,
        93,
        14,
        0);
}

static int
run_builtin_loader_bmp_fail_bi_jpeg_payload_num_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail bi-jpeg invalid payload numeric",
        "/tests/data/inputs/formats/bmp-bad-bi-jpeg-invalid-payload.bmp");
}

static int
run_builtin_loader_bmp_fail_bi_png_payload_range_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail bi-png payload range numeric",
        "/tests/data/inputs/formats/bmp-bad-bi-png-payload-range.bmp");
}

static int
verify_bmp_float_probe_metadata(char const *label,
                                bmp_numeric_probe_context_t const *probe,
                                int expected_width,
                                int expected_height,
                                int expect_mask)
{
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    if (label == NULL || probe == NULL) {
        return 1;
    }
    if (probe->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch (%d)\n",
                label,
                probe->callback_count);
        return 1;
    }
    if (!(probe->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
          probe->pixelformat == SIXEL_PIXELFORMAT_RGB888)) {
        fprintf(stderr, "%s: pixelformat mismatch (%d)\n",
                label,
                probe->pixelformat);
        return 1;
    }
    if ((probe->pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 &&
         probe->colorspace != SIXEL_COLORSPACE_LINEAR) ||
        (probe->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
         probe->colorspace != SIXEL_COLORSPACE_GAMMA)) {
        fprintf(stderr, "%s: colorspace mismatch (%d)\n",
                label,
                probe->colorspace);
        return 1;
    }
    if (probe->width != expected_width || probe->height != expected_height) {
        fprintf(stderr, "%s: geometry mismatch (%dx%d)\n",
                label,
                probe->width,
                probe->height);
        return 1;
    }
    if (expect_mask) {
        if (probe->alpha_zero_is_transparent != 1 ||
            probe->has_transparent_mask != 1 ||
            probe->transparent_mask_size < sizeof(expected_mask)) {
            fprintf(stderr, "%s: transparent mask metadata mismatch\n",
                    label);
            return 1;
        }
        if (memcmp(probe->transparent_mask,
                   expected_mask,
                   sizeof(expected_mask)) != 0) {
            fprintf(stderr, "%s: transparent mask samples mismatch\n",
                    label);
            return 1;
        }
    } else if (probe->alpha_zero_is_transparent != 0 ||
               probe->has_transparent_mask != 0 ||
               probe->transparent_mask_size != 0u) {
        fprintf(stderr, "%s: unexpected transparent mask (%d, %zu)\n",
                label,
                probe->has_transparent_mask,
                probe->transparent_mask_size);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_v5_embedded_icc_rgb_cms_on_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;
    int target_pixelformat;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;
    target_pixelformat = SIXEL_PIXELFORMAT_RGB888;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp v5 embedded icc rgb cms on numeric",
        "/tests/data/inputs/formats/bmp-v5-embedded-icc-rgb-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgb cms on numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }

    target_pixelformat = loader_cms_target_pixelformat();
    if (target_pixelformat == SIXEL_PIXELFORMAT_RGB888) {
        return verify_bmp_rgb_probe(
            "builtin loader bmp v5 embedded icc rgb cms on numeric",
            &probe,
            2,
            2,
            expected_rgb,
            sizeof(expected_rgb));
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgb cms on numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != target_pixelformat) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgb cms on numeric: "
                "target pixelformat mismatch (%d != %d)\n",
                probe.pixelformat,
                target_pixelformat);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgb cms on numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 0 ||
        probe.has_transparent_mask != 0 ||
        probe.transparent_mask_size != 0u) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgb cms on numeric: "
                "unexpected transparency metadata\n");
        return 1;
    }
    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(probe.pixelformat)) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgb cms on numeric: "
                "unexpected non-float pixelformat (%d)\n",
                probe.pixelformat);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_v5_embedded_icc_rgb_cms_off_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp v5 embedded icc rgb cms off numeric",
        "/tests/data/inputs/formats/bmp-v5-embedded-icc-rgb-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_v5_icc_rgba_bgcolor_num_test(void)
{
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp v5 embedded icc rgba bgcolor numeric",
        "/tests/data/inputs/formats/bmp-v5-embedded-icc-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgba bgcolor numeric: "
                "loader failed (%d)\n",
                (int)status);
        result = 1;
        goto end;
    }
    result = verify_bmp_float_probe_metadata(
        "builtin loader bmp v5 embedded icc rgba bgcolor numeric",
        &probe,
        2,
        2,
        1);

end:
    sixel_helper_set_loader_background_colorspace(-1);
    return result;
}

static int
run_builtin_loader_bmp_v5_icc_rgba_mask_no_bg_num_test(void)
{
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp v5 embedded icc rgba mask no-bg numeric",
        "/tests/data/inputs/formats/bmp-v5-embedded-icc-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgba mask no-bg numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgba mask no-bg numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    if (probe.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgba mask no-bg numeric: "
                "unexpected format/colorspace (%d, %d)\n",
                probe.pixelformat,
                probe.colorspace);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgba mask no-bg numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.alpha_zero_is_transparent != 1 ||
        probe.has_transparent_mask != 1 ||
        probe.transparent_mask_size < sizeof(expected_mask)) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgba mask no-bg numeric: "
                "transparent-mask metadata mismatch\n");
        return 1;
    }
    if (memcmp(probe.transparent_mask,
               expected_mask,
               sizeof(expected_mask)) != 0) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc rgba mask no-bg numeric: "
                "transparent-mask samples mismatch\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_v5_linked_profile_ignored_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp v5 linked profile ignored numeric",
        "/tests/data/inputs/formats/bmp-v5-linked-profile-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_fail_v5_embedded_icc_range_numeric_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    sixel_helper_set_additional_message(NULL);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp fail v5 embedded icc range numeric",
        "/tests/data/inputs/formats/bmp-bad-v5-embedded-icc-range.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr,
                "builtin loader bmp fail v5 embedded icc range numeric: "
                "unexpected success\n");
        return 1;
    }
    if (probe.callback_count != 0) {
        fprintf(stderr,
                "builtin loader bmp fail v5 embedded icc range numeric: "
                "unexpected callback (%d)\n",
                probe.callback_count);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_alphabitfields_bgcolor_numeric_test(void)
{
    static unsigned char const src_rgba_topdown[16] = {
        0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x80u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu, 0x00u, 0x00u
    };
    static unsigned char const bgcolor_u8[3] = { 0x20u, 0x40u, 0x80u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    float expected_linear[12];
    float bg_linear[3];
    int result;
    int channel;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(expected_linear, 0, sizeof(expected_linear));
    memset(bg_linear, 0, sizeof(bg_linear));
    result = 1;
    channel = 0;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 1;
    options.bgcolor = bgcolor_u8;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_LINEAR);
    result = run_builtin_loader_probe_case(
        "builtin loader bmp alphabitfields bgcolor numeric",
        "/tests/data/inputs/formats/bmp-info40-alphabitfields-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    sixel_helper_set_loader_background_colorspace(-1);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp alphabitfields bgcolor numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    for (channel = 0; channel < 3; ++channel) {
        bg_linear[channel] = (float)bgcolor_u8[channel] / 255.0f;
    }
    bmp_numeric_compose_expected_linear(expected_linear,
                                        src_rgba_topdown,
                                        bg_linear);

    return verify_bmp_float_probe(
        "builtin loader bmp alphabitfields bgcolor numeric",
        &probe,
        expected_linear,
        0.00001f);
}

static int
run_builtin_loader_bmp_alphabitfields_mask_no_bg_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0x00u
    };
    static unsigned char const expected_mask[4] = { 0u, 0u, 0u, 1u };
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp alphabitfields mask no-bg numeric",
        "/tests/data/inputs/formats/bmp-info40-alphabitfields-rgba-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp alphabitfields mask no-bg numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }

    return verify_bmp_rgb_mask_probe(
        "builtin loader bmp alphabitfields mask no-bg numeric",
        &probe,
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb),
        expected_mask,
        sizeof(expected_mask));
}

static int
run_builtin_loader_bmp_fail_abf_invalid_mask_num_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail alphabitfields invalid alpha mask numeric",
        "/tests/data/inputs/formats/"
        "bmp-bad-info40-alphabitfields-zero-alpha-mask.bmp");
}

static int
run_builtin_loader_bmp_v4_calibrated_cms_on_numeric_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "1") != 0) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated cms on numeric: "
                "setenv failed\n");
        return 1;
    }
    result = run_builtin_loader_probe_case(
        "builtin loader bmp v4 calibrated cms on numeric",
        "/tests/data/inputs/formats/bmp-v4-calibrated-rgb-24bpp-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "") != 0) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated cms on numeric: "
                "reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated cms on numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1 ||
        probe.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        probe.colorspace != SIXEL_COLORSPACE_GAMMA ||
        probe.width != 2 ||
        probe.height != 2) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated cms on numeric: "
                "metadata mismatch\n");
        return 1;
    }
    if (probe.has_transparent_mask != 0 ||
        probe.transparent_mask_size != 0u ||
        probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated cms on numeric: "
                "unexpected transparency metadata\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_v4_calibrated_cms_off_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0x80u, 0x40u, 0x20u, 0xffu, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u, 0x00u, 0x00u, 0xffu
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp v4 calibrated cms off numeric",
        "/tests/data/inputs/formats/bmp-v4-calibrated-rgb-24bpp-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_v4_cal_probe_split_gamma_num_test(void)
{
    sixel_frombmp_probe_t probe;
    SIXELSTATUS status;
    double expected_average;
    int result;

    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    expected_average = 0.0;
    result = 1;

    result = run_builtin_loader_bmp_probe_fixture_case(
        "builtin loader bmp v4 calibrated probe split gamma numeric",
        "/tests/data/inputs/formats/"
        "bmp-v4-calibrated-rgb-split-gamma-24bpp-2x2.bmp",
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated probe split gamma numeric: "
                "probe failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.has_calibrated_rgb == 0) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated probe split gamma numeric: "
                "missing calibrated metadata\n");
        return 1;
    }

    expected_average = (1.0 + 2.0 + 3.0) / 3.0;
    if (fabs(probe.calibrated_gamma_r - 1.0) > 1.0e-8 ||
        fabs(probe.calibrated_gamma_g - 2.0) > 1.0e-8 ||
        fabs(probe.calibrated_gamma_b - 3.0) > 1.0e-8 ||
        fabs(probe.calibrated_gamma - expected_average) > 1.0e-8) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated probe split gamma numeric: "
                "unexpected gamma values (%0.8f,%0.8f,%0.8f avg=%0.8f)\n",
                probe.calibrated_gamma_r,
                probe.calibrated_gamma_g,
                probe.calibrated_gamma_b,
                probe.calibrated_gamma);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2 || probe.bpp != 24) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated probe split gamma numeric: "
                "geometry/depth mismatch (%dx%d bpp=%d)\n",
                probe.width,
                probe.height,
                probe.bpp);
        return 1;
    }

    return 0;
}

static int
run_bmp_v4_cal_split_gamma_cms_on_num_t(void)
{
    builtin_loader_probe_options_t options_off;
    builtin_loader_probe_options_t options_on;
    bmp_numeric_probe_context_t probe_off;
    bmp_numeric_probe_context_t probe_on;
    sixel_frombmp_probe_t bmp_probe;
    sixel_cms_profile_t *profile_per_ch;
    sixel_cms_profile_t *profile_avg;
    SIXELSTATUS status;
    unsigned char expected_per_ch[12];
    unsigned char expected_avg[12];
    sixel_cms_engine_t saved_engine;
    int result;
    size_t index;
    int differs_from_avg;

    status = SIXEL_FALSE;
    memset(&options_off, 0, sizeof(options_off));
    memset(&options_on, 0, sizeof(options_on));
    memset(&probe_off, 0, sizeof(probe_off));
    memset(&probe_on, 0, sizeof(probe_on));
    memset(&bmp_probe, 0, sizeof(bmp_probe));
    memset(expected_per_ch, 0, sizeof(expected_per_ch));
    memset(expected_avg, 0, sizeof(expected_avg));
    saved_engine = SIXEL_CMS_ENGINE_AUTO;
    profile_per_ch = NULL;
    profile_avg = NULL;
    result = 1;
    index = 0u;
    differs_from_avg = 0;

    options_off.require_static = 1;
    options_off.use_palette = 0;
    options_off.reqcolors = 256;
    options_off.set_bgcolor = 0;
    options_off.bgcolor = NULL;
    options_off.set_loop_control = 0;
    options_off.loop_control = SIXEL_LOOP_AUTO;
    options_off.set_cms_engine = 0;
    options_off.cms_engine = SIXEL_CMS_ENGINE_NONE;

    options_on = options_off;
    options_on.set_cms_engine = 1;
    options_on.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    result = run_builtin_loader_probe_case(
        "builtin loader bmp v4 calibrated split-gamma cms off baseline",
        "/tests/data/inputs/formats/"
        "bmp-v4-calibrated-rgb-split-gamma-24bpp-2x2.bmp",
        &options_off,
        capture_bmp_numeric_probe,
        &probe_off,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms off "
                "baseline: loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe_off.callback_count != 1 ||
        probe_off.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        probe_off.colorspace != SIXEL_COLORSPACE_GAMMA ||
        probe_off.width != 2 ||
        probe_off.height != 2) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms off "
                "baseline: metadata mismatch\n");
        return 1;
    }

    result = run_builtin_loader_bmp_probe_fixture_case(
        "builtin loader bmp v4 calibrated split-gamma cms probe",
        "/tests/data/inputs/formats/"
        "bmp-v4-calibrated-rgb-split-gamma-24bpp-2x2.bmp",
        &bmp_probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status) || bmp_probe.has_calibrated_rgb == 0) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms probe: "
                "missing calibrated metadata (%d)\n",
                (int)status);
        return 1;
    }
    if (bmp_probe.icc_profile != NULL || bmp_probe.icc_profile_length != 0u) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms probe: "
                "unexpected embedded ICC metadata (%zu)\n",
                bmp_probe.icc_profile_length);
        return 1;
    }

    memcpy(expected_per_ch, probe_off.pixels_u8, sizeof(expected_per_ch));
    memcpy(expected_avg, probe_off.pixels_u8, sizeof(expected_avg));

    saved_engine = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);

    profile_per_ch = sixel_cms_create_rgb_profile_from_gammas_chrm(
        bmp_probe.calibrated_gamma_r,
        bmp_probe.calibrated_gamma_g,
        bmp_probe.calibrated_gamma_b,
        bmp_probe.white_x,
        bmp_probe.white_y,
        bmp_probe.red_x,
        bmp_probe.red_y,
        bmp_probe.green_x,
        bmp_probe.green_y,
        bmp_probe.blue_x,
        bmp_probe.blue_y);
    if (profile_per_ch == NULL) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: per-channel profile creation failed\n");
        sixel_cms_set_engine(saved_engine);
        return 1;
    }
    if (!sixel_cms_convert_profile_to_srgb(expected_per_ch,
                                           2,
                                           2,
                                           SIXEL_PIXELFORMAT_RGB888,
                                           profile_per_ch)) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: per-channel conversion failed\n");
        sixel_cms_close_profile(profile_per_ch);
        sixel_cms_set_engine(saved_engine);
        return 1;
    }
    sixel_cms_close_profile(profile_per_ch);
    profile_per_ch = NULL;

    profile_avg = sixel_cms_create_rgb_profile_from_gamma_chrm(
        bmp_probe.calibrated_gamma,
        bmp_probe.white_x,
        bmp_probe.white_y,
        bmp_probe.red_x,
        bmp_probe.red_y,
        bmp_probe.green_x,
        bmp_probe.green_y,
        bmp_probe.blue_x,
        bmp_probe.blue_y);
    if (profile_avg == NULL) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: avg profile creation failed\n");
        sixel_cms_set_engine(saved_engine);
        return 1;
    }
    if (!sixel_cms_convert_profile_to_srgb(expected_avg,
                                           2,
                                           2,
                                           SIXEL_PIXELFORMAT_RGB888,
                                           profile_avg)) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: avg conversion failed\n");
        sixel_cms_close_profile(profile_avg);
        sixel_cms_set_engine(saved_engine);
        return 1;
    }
    sixel_cms_close_profile(profile_avg);
    profile_avg = NULL;
    sixel_cms_set_engine(saved_engine);

    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "1") != 0) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: setenv failed\n");
        return 1;
    }
    result = run_builtin_loader_probe_case(
        "builtin loader bmp v4 calibrated split-gamma cms on numeric",
        "/tests/data/inputs/formats/"
        "bmp-v4-calibrated-rgb-split-gamma-24bpp-2x2.bmp",
        &options_on,
        capture_bmp_numeric_probe,
        &probe_on,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "") != 0) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe_on.callback_count != 1 ||
        probe_on.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        probe_on.colorspace != SIXEL_COLORSPACE_GAMMA ||
        probe_on.width != 2 ||
        probe_on.height != 2) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: metadata mismatch\n");
        return 1;
    }
    if (memcmp(probe_on.pixels_u8, expected_per_ch, sizeof(expected_per_ch))
        != 0) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: result differs from per-channel reference\n");
        return 1;
    }

    for (index = 0u; index < sizeof(expected_avg); ++index) {
        if (probe_on.pixels_u8[index] != expected_avg[index]) {
            differs_from_avg = 1;
            break;
        }
    }
    if (!differs_from_avg) {
        fprintf(stderr,
                "builtin loader bmp v4 calibrated split-gamma cms on "
                "numeric: unexpectedly matched average-gamma reference\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_v5_icc_calibrated_priority_num_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "1") != 0) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc calibrated priority "
                "numeric: setenv failed\n");
        return 1;
    }
    result = run_builtin_loader_probe_case(
        "builtin loader bmp v5 embedded icc calibrated priority numeric",
        "/tests/data/inputs/formats/"
        "bmp-v5-embedded-icc-calibrated-rgb-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "") != 0) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc calibrated priority "
                "numeric: reset env failed\n");
        return 1;
    }
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc calibrated priority "
                "numeric: loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1 ||
        probe.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        probe.colorspace != SIXEL_COLORSPACE_GAMMA ||
        probe.width != 2 ||
        probe.height != 2) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc calibrated priority "
                "numeric: metadata mismatch\n");
        return 1;
    }
    if (probe.has_transparent_mask != 0 ||
        probe.transparent_mask_size != 0u ||
        probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr,
                "builtin loader bmp v5 embedded icc calibrated priority "
                "numeric: unexpected transparency metadata\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_cmyk_cms_off_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0xffu, 0xffu,
        0xffu, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };

    return run_builtin_loader_bmp_rgb_fixture_case(
        "builtin loader bmp cmyk cms off numeric",
        "/tests/data/inputs/formats/bmp-info40-cmyk-32bpp-2x2.bmp",
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_fail_cmyk_topdown_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail cmyk topdown numeric",
        "/tests/data/inputs/formats/bmp-info40-cmyk-32bpp-topdown-2x2.bmp");
}

static int
run_builtin_loader_bmp_cmyk_embedded_icc_cms_on_numeric_test(void)
{
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    int result;
    int target_pixelformat;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    result = 1;
    target_pixelformat = SIXEL_PIXELFORMAT_RGB888;

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;

    if (loader_test_setenv("SIXEL_LOADER_PREFER_8BIT", "") != 0) {
        fprintf(stderr,
                "builtin loader bmp cmyk embedded icc cms on numeric: "
                "setenv failed\n");
        return 1;
    }
    result = run_builtin_loader_probe_case(
        "builtin loader bmp cmyk embedded icc cms on numeric",
        "/tests/data/inputs/formats/bmp-v5-cmyk-embedded-icc-2x2.bmp",
        &options,
        capture_bmp_numeric_probe,
        &probe,
        &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "builtin loader bmp cmyk embedded icc cms on numeric: "
                "loader failed (%d)\n",
                (int)status);
        return 1;
    }
    if (probe.callback_count != 1) {
        fprintf(stderr,
                "builtin loader bmp cmyk embedded icc cms on numeric: "
                "callback count mismatch (%d)\n",
                probe.callback_count);
        return 1;
    }
    target_pixelformat = loader_cms_target_pixelformat();
    if (probe.pixelformat != target_pixelformat) {
        fprintf(stderr,
                "builtin loader bmp cmyk embedded icc cms on numeric: "
                "pixelformat mismatch (%d != %d)\n",
                probe.pixelformat,
                target_pixelformat);
        return 1;
    }
    if (probe.width != 2 || probe.height != 2) {
        fprintf(stderr,
                "builtin loader bmp cmyk embedded icc cms on numeric: "
                "geometry mismatch (%dx%d)\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.has_transparent_mask != 0 ||
        probe.transparent_mask_size != 0u ||
        probe.alpha_zero_is_transparent != 0) {
        fprintf(stderr,
                "builtin loader bmp cmyk embedded icc cms on numeric: "
                "unexpected transparency metadata\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_bmp_fail_cmyk_requires_32bpp_numeric_test(void)
{
    return run_builtin_loader_bmp_expect_fail_case(
        "builtin loader bmp fail cmyk requires 32bpp numeric",
        "/tests/data/inputs/formats/bmp-bad-info40-cmyk-24bpp.bmp");
}

static int
run_builtin_loader_bmp_cmykrle8_decode_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0xffu, 0xffu,
        0xffu, 0x00u, 0xffu, 0xffu, 0xffu, 0x00u
    };
    static unsigned char const bmp_cmykrle8_sample[] = {
        0x42u, 0x4du, 0x54u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x46u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x08u, 0x00u, 0x0cu, 0x00u,
        0x00u, 0x00u, 0x0eu, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu,
        0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u, 0x01u, 0x02u,
        0x01u, 0x03u, 0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x01u
    };

    return run_builtin_loader_bmp_rgb_buffer_case(
        "builtin loader bmp cmykrle8 decode numeric",
        bmp_cmykrle8_sample,
        sizeof(bmp_cmykrle8_sample),
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_cmykrle4_decode_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0xffu, 0xffu,
        0xffu, 0x00u, 0xffu, 0xffu, 0xffu, 0x00u
    };
    static unsigned char const bmp_cmykrle4_sample[] = {
        0x42u, 0x4du, 0x54u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x46u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x04u, 0x00u, 0x0du, 0x00u,
        0x00u, 0x00u, 0x0eu, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu,
        0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u, 0x02u, 0x23u,
        0x00u, 0x00u, 0x02u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x00u
    };

    return run_builtin_loader_bmp_rgb_buffer_case(
        "builtin loader bmp cmykrle4 decode numeric",
        bmp_cmykrle4_sample,
        sizeof(bmp_cmykrle4_sample),
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_fail_cmykrle8_requires_8bpp_numeric_test(void)
{
    static unsigned char const bmp_cmykrle8_bad_bpp_sample[] = {
        0x42u, 0x4du, 0x3au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x04u, 0x00u, 0x0cu, 0x00u,
        0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x01u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail cmykrle8 requires 8bpp numeric",
        bmp_cmykrle8_bad_bpp_sample,
        sizeof(bmp_cmykrle8_bad_bpp_sample));
}

static int
run_builtin_loader_bmp_fail_cmykrle4_requires_4bpp_numeric_test(void)
{
    static unsigned char const bmp_cmykrle4_bad_bpp_sample[] = {
        0x42u, 0x4du, 0x3au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x08u, 0x00u, 0x0du, 0x00u,
        0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x01u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail cmykrle4 requires 4bpp numeric",
        bmp_cmykrle4_bad_bpp_sample,
        sizeof(bmp_cmykrle4_bad_bpp_sample));
}

static int
run_builtin_loader_bmp_fail_cmykrle8_broken_stream_numeric_test(void)
{
    static unsigned char const bmp_cmykrle8_broken_stream_sample[] = {
        0x42u, 0x4du, 0x4au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x46u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x08u, 0x00u, 0x0cu, 0x00u,
        0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu,
        0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x03u,
        0x00u, 0x01u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail cmykrle8 broken stream numeric",
        bmp_cmykrle8_broken_stream_sample,
        sizeof(bmp_cmykrle8_broken_stream_sample));
}

static int
run_builtin_loader_bmp_fail_cmykrle4_broken_stream_numeric_test(void)
{
    static unsigned char const bmp_cmykrle4_broken_stream_sample[] = {
        0x42u, 0x4du, 0x4au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x46u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x04u, 0x00u, 0x0du, 0x00u,
        0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu,
        0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x02u,
        0x03u, 0x00u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail cmykrle4 broken stream numeric",
        bmp_cmykrle4_broken_stream_sample,
        sizeof(bmp_cmykrle4_broken_stream_sample));
}

static int
run_builtin_loader_bmp_fail_cmykrle8_topdown_numeric_test(void)
{
    static unsigned char const bmp_cmykrle8_topdown_sample[] = {
        0x42u, 0x4du, 0x3au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0xfeu, 0xffu,
        0xffu, 0xffu, 0x01u, 0x00u, 0x08u, 0x00u, 0x0cu, 0x00u,
        0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x01u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail cmykrle8 topdown numeric",
        bmp_cmykrle8_topdown_sample,
        sizeof(bmp_cmykrle8_topdown_sample));
}

static int
run_builtin_loader_bmp_fail_cmykrle4_topdown_numeric_test(void)
{
    static unsigned char const bmp_cmykrle4_topdown_sample[] = {
        0x42u, 0x4du, 0x3au, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x36u, 0x00u, 0x00u, 0x00u, 0x28u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0xfeu, 0xffu,
        0xffu, 0xffu, 0x01u, 0x00u, 0x04u, 0x00u, 0x0du, 0x00u,
        0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x01u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail cmykrle4 topdown numeric",
        bmp_cmykrle4_topdown_sample,
        sizeof(bmp_cmykrle4_topdown_sample));
}

static int
run_builtin_loader_bmp_os2_rgb24_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const bmp_os2_rgb24_sample[] = {
        0x42u, 0x4du, 0x5eu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x4eu, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x18u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x10u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0xffu, 0x00u, 0x00u, 0x00u
    };

    return run_builtin_loader_bmp_rgb_buffer_case(
        "builtin loader bmp os2 rgb24 numeric",
        bmp_os2_rgb24_sample,
        sizeof(bmp_os2_rgb24_sample),
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_os2_rle8_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const bmp_os2_rle8_sample[] = {
        0x42u, 0x4du, 0x6cu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x5eu, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x08u, 0x00u, 0x01u, 0x00u,
        0x00u, 0x00u, 0x0eu, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x02u,
        0x01u, 0x03u, 0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x01u
    };

    return run_builtin_loader_bmp_rgb_buffer_case(
        "builtin loader bmp os2 rle8 numeric",
        bmp_os2_rle8_sample,
        sizeof(bmp_os2_rle8_sample),
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_os2_rle4_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const bmp_os2_rle4_sample[] = {
        0x42u, 0x4du, 0x68u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x5eu, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x04u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x0au, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x02u, 0x23u,
        0x00u, 0x00u, 0x02u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u
    };

    return run_builtin_loader_bmp_rgb_buffer_case(
        "builtin loader bmp os2 rle4 numeric",
        bmp_os2_rle4_sample,
        sizeof(bmp_os2_rle4_sample),
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_os2_huffman1d_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const bmp_os2_huffman1d_sample[] = {
        0x42u, 0x4du, 0x5cu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x56u, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u, 0x03u, 0x00u,
        0x00u, 0x00u, 0x06u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu,
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x35u, 0xc0u,
        0x04u, 0x74u, 0x00u, 0x20u
    };

    return run_builtin_loader_bmp_rgb_buffer_case(
        "builtin loader bmp os2 huffman1d numeric",
        bmp_os2_huffman1d_sample,
        sizeof(bmp_os2_huffman1d_sample),
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_os2_rle24_numeric_test(void)
{
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const bmp_os2_rle24_sample[] = {
        0x42u, 0x4du, 0x64u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x4eu, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x18u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x16u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0xffu,
        0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x01u, 0x00u, 0x00u, 0xffu, 0x01u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0x00u, 0x01u
    };

    return run_builtin_loader_bmp_rgb_buffer_case(
        "builtin loader bmp os2 rle24 numeric",
        bmp_os2_rle24_sample,
        sizeof(bmp_os2_rle24_sample),
        2,
        2,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_fail_os2_rle24_truncated_numeric_test(void)
{
    static unsigned char const bmp_os2_rle24_truncated_sample[] = {
        0x42u, 0x4du, 0x51u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x4eu, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x18u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0xffu,
        0x00u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail os2 rle24 truncated numeric",
        bmp_os2_rle24_truncated_sample,
        sizeof(bmp_os2_rle24_truncated_sample));
}

static int
run_builtin_loader_bmp_fail_os2_huffman1d_invalid_numeric_test(void)
{
    static unsigned char const bmp_os2_huffman1d_invalid_sample[] = {
        0x42u, 0x4du, 0x58u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x56u, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u, 0x03u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu,
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail os2 huffman1d invalid numeric",
        bmp_os2_huffman1d_invalid_sample,
        sizeof(bmp_os2_huffman1d_invalid_sample));
}

static int
run_builtin_loader_bmp_fail_os2_rle24_abs_ov_num_test(void)
{
    static unsigned char const bmp_os2_rle24_absolute_overflow_sample[] = {
        0x42u, 0x4du, 0x53u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x4eu, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x18u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x05u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x02u,
        0x00u, 0x00u, 0xffu
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail os2 rle24 absolute overflow numeric",
        bmp_os2_rle24_absolute_overflow_sample,
        sizeof(bmp_os2_rle24_absolute_overflow_sample));
}

static int
run_builtin_loader_bmp_fail_os2_rle24_delta_numeric_test(void)
{
    static unsigned char const bmp_os2_rle24_delta_sample[] = {
        0x42u, 0x4du, 0x52u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x4eu, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x02u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x18u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x04u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x02u,
        0x03u, 0x00u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail os2 rle24 delta numeric",
        bmp_os2_rle24_delta_sample,
        sizeof(bmp_os2_rle24_delta_sample));
}

static int
run_builtin_loader_bmp_fail_os2_rle24_topdown_numeric_test(void)
{
    static unsigned char const bmp_os2_rle24_topdown_sample[] = {
        0x42u, 0x4du, 0x50u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x4eu, 0x00u, 0x00u, 0x00u, 0x40u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0xfeu, 0xffu,
        0xffu, 0xffu, 0x01u, 0x00u, 0x18u, 0x00u, 0x04u, 0x00u,
        0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x13u, 0x0bu,
        0x00u, 0x00u, 0x13u, 0x0bu, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u
    };

    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail os2 rle24 topdown numeric",
        bmp_os2_rle24_topdown_sample,
        sizeof(bmp_os2_rle24_topdown_sample));
}

#define BMP_NUMERIC_OS2_HUFFMAN1D_PIXEL_OFFSET 86u
#define BMP_NUMERIC_OS2_HUFFMAN1D_MAX_BMP_SIZE 1024u
#define BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD 256u
#define BMP_NUMERIC_V2_DIB_SIZE 52u
#define BMP_NUMERIC_V2_HEADER_SIZE \
    (14u + BMP_NUMERIC_V2_DIB_SIZE)
#define BMP_NUMERIC_V2_MAX_BMP_SIZE 512u

static void
bmp_numeric_write_u16le(unsigned char *buffer,
                        size_t offset,
                        unsigned int value)
{
    if (buffer == NULL) {
        return;
    }
    buffer[offset + 0u] = (unsigned char)(value & 0xffu);
    buffer[offset + 1u] = (unsigned char)((value >> 8u) & 0xffu);
}

static void
bmp_numeric_write_u32le(unsigned char *buffer,
                        size_t offset,
                        uint32_t value)
{
    if (buffer == NULL) {
        return;
    }
    buffer[offset + 0u] = (unsigned char)(value & 0xffu);
    buffer[offset + 1u] = (unsigned char)((value >> 8u) & 0xffu);
    buffer[offset + 2u] = (unsigned char)((value >> 16u) & 0xffu);
    buffer[offset + 3u] = (unsigned char)((value >> 24u) & 0xffu);
}

static int
bmp_num_mk_bi_png40(unsigned char *bmp,
                    size_t bmp_capacity,
                    unsigned int bpp,
                    unsigned char const *payload,
                    size_t payload_size,
                    size_t *bmp_size)
{
    size_t pixel_offset;
    size_t file_size;
    uint32_t file_size_u32;
    uint32_t image_size_u32;

    pixel_offset = 54u;
    file_size = 0u;
    file_size_u32 = 0u;
    image_size_u32 = 0u;
    if (bmp == NULL || payload == NULL || bmp_size == NULL) {
        return 0;
    }
    if (pixel_offset > SIZE_MAX - payload_size) {
        return 0;
    }
    file_size = pixel_offset + payload_size;
    if (file_size > bmp_capacity || file_size > 0xffffffffu ||
        payload_size > 0xffffffffu) {
        return 0;
    }

    file_size_u32 = (uint32_t)file_size;
    image_size_u32 = (uint32_t)payload_size;
    memset(bmp, 0, file_size);
    bmp[0] = 0x42u;
    bmp[1] = 0x4du;
    bmp_numeric_write_u32le(bmp, 2u, file_size_u32);
    bmp_numeric_write_u32le(bmp, 10u, (uint32_t)pixel_offset);
    bmp_numeric_write_u32le(bmp, 14u, 40u);
    bmp_numeric_write_u32le(bmp, 18u, 2u);
    bmp_numeric_write_u32le(bmp, 22u, 2u);
    bmp_numeric_write_u16le(bmp, 26u, 1u);
    bmp_numeric_write_u16le(bmp, 28u, bpp);
    bmp_numeric_write_u32le(bmp, 30u, 5u);
    bmp_numeric_write_u32le(bmp, 34u, image_size_u32);
    bmp_numeric_write_u32le(bmp, 38u, 0x00000b13u);
    bmp_numeric_write_u32le(bmp, 42u, 0x00000b13u);
    memcpy(bmp + pixel_offset, payload, payload_size);
    *bmp_size = file_size;

    return 1;
}

static int
bmp_numeric_build_v2_bmp(unsigned char *bmp,
                         size_t bmp_capacity,
                         int width,
                         int height_signed,
                         unsigned int bpp,
                         unsigned int compression,
                         unsigned int red_mask,
                         unsigned int green_mask,
                         unsigned int blue_mask,
                         size_t pixel_offset,
                         unsigned char const *payload,
                         size_t payload_size,
                         size_t *bmp_size)
{
    size_t file_size;
    uint32_t file_size_u32;
    uint32_t pixel_offset_u32;

    file_size = 0u;
    file_size_u32 = 0u;
    pixel_offset_u32 = 0u;
    if (bmp == NULL || payload == NULL || bmp_size == NULL) {
        return 0;
    }
    if (width <= 0 || height_signed == 0) {
        return 0;
    }
    if (pixel_offset > SIZE_MAX - payload_size) {
        return 0;
    }
    file_size = pixel_offset + payload_size;
    if (file_size > bmp_capacity || file_size > 0xffffffffu ||
        pixel_offset > 0xffffffffu) {
        return 0;
    }
    file_size_u32 = (uint32_t)file_size;
    pixel_offset_u32 = (uint32_t)pixel_offset;

    memset(bmp, 0, file_size);
    bmp[0] = 0x42u;
    bmp[1] = 0x4du;
    bmp_numeric_write_u32le(bmp, 2u, file_size_u32);
    bmp_numeric_write_u32le(bmp, 10u, pixel_offset_u32);
    bmp_numeric_write_u32le(bmp, 14u, BMP_NUMERIC_V2_DIB_SIZE);
    bmp_numeric_write_u32le(bmp, 18u, (uint32_t)(unsigned int)width);
    bmp_numeric_write_u32le(bmp, 22u, (uint32_t)height_signed);
    bmp_numeric_write_u16le(bmp, 26u, 1u);
    bmp_numeric_write_u16le(bmp, 28u, bpp);
    bmp_numeric_write_u32le(bmp, 30u, compression);
    bmp_numeric_write_u32le(bmp, 34u, (uint32_t)payload_size);
    bmp_numeric_write_u32le(bmp, 38u, 0x00000b13u);
    bmp_numeric_write_u32le(bmp, 42u, 0x00000b13u);
    bmp_numeric_write_u32le(bmp, 54u, red_mask);
    bmp_numeric_write_u32le(bmp, 58u, green_mask);
    bmp_numeric_write_u32le(bmp, 62u, blue_mask);
    memcpy(bmp + pixel_offset, payload, payload_size);
    *bmp_size = file_size;

    return 1;
}

static int
run_builtin_loader_bmp_v2_rgb_buffer_case(char const *label,
                                           unsigned int bpp,
                                           unsigned int compression,
                                           unsigned int red_mask,
                                           unsigned int green_mask,
                                           unsigned int blue_mask,
                                           size_t pixel_offset,
                                           unsigned char const *payload,
                                           size_t payload_size,
                                           unsigned char const *expected_rgb,
                                           size_t expected_rgb_size)
{
    unsigned char bmp[BMP_NUMERIC_V2_MAX_BMP_SIZE];
    size_t bmp_size;

    bmp_size = 0u;
    memset(bmp, 0, sizeof(bmp));
    if (!bmp_numeric_build_v2_bmp(bmp,
                                  sizeof(bmp),
                                  2,
                                  2,
                                  bpp,
                                  compression,
                                  red_mask,
                                  green_mask,
                                  blue_mask,
                                  pixel_offset,
                                  payload,
                                  payload_size,
                                  &bmp_size)) {
        fprintf(stderr, "%s: failed to build V2 BMP buffer\n", label);
        return 1;
    }
    return run_builtin_loader_bmp_rgb_buffer_case(label,
                                                  bmp,
                                                  bmp_size,
                                                  2,
                                                  2,
                                                  expected_rgb,
                                                  expected_rgb_size);
}

static int
run_builtin_loader_bmp_v2_fail_buffer_case(char const *label,
                                            unsigned int bpp,
                                            unsigned int compression,
                                            unsigned int red_mask,
                                            unsigned int green_mask,
                                            unsigned int blue_mask,
                                            size_t pixel_offset,
                                            unsigned char const *payload,
                                            size_t payload_size)
{
    unsigned char bmp[BMP_NUMERIC_V2_MAX_BMP_SIZE];
    size_t bmp_size;

    bmp_size = 0u;
    memset(bmp, 0, sizeof(bmp));
    if (!bmp_numeric_build_v2_bmp(bmp,
                                  sizeof(bmp),
                                  2,
                                  2,
                                  bpp,
                                  compression,
                                  red_mask,
                                  green_mask,
                                  blue_mask,
                                  pixel_offset,
                                  payload,
                                  payload_size,
                                  &bmp_size)) {
        fprintf(stderr, "%s: failed to build V2 BMP buffer\n", label);
        return 1;
    }
    return run_builtin_loader_bmp_expect_fail_buffer_case(label,
                                                          bmp,
                                                          bmp_size);
}

static int
run_builtin_loader_bmp_v2_16bpp_bitfields_rgb565_numeric_test(void)
{
    static unsigned char const payload[] = {
        0x1fu, 0x00u, 0xffu, 0xffu, 0x00u, 0xf8u, 0xe0u, 0x07u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_v2_rgb_buffer_case(
        "builtin loader bmp v2 16bpp bitfields rgb565 numeric",
        16u,
        3u,
        0x0000f800u,
        0x000007e0u,
        0x0000001fu,
        BMP_NUMERIC_V2_HEADER_SIZE,
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_v2_32bpp_bitfields_no_alpha_numeric_test(void)
{
    static unsigned char const payload[] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0xffu, 0x00u, 0x00u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };

    return run_builtin_loader_bmp_v2_rgb_buffer_case(
        "builtin loader bmp v2 32bpp bitfields no alpha numeric",
        32u,
        3u,
        0x00ff0000u,
        0x0000ff00u,
        0x000000ffu,
        BMP_NUMERIC_V2_HEADER_SIZE,
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_v2_16bpp_rgb555_numeric_test(void)
{
    static unsigned char const payload[] = {
        0x1fu, 0x00u, 0xffu, 0x7fu, 0x00u, 0x7cu, 0xe0u, 0x03u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    return run_builtin_loader_bmp_v2_rgb_buffer_case(
        "builtin loader bmp v2 16bpp rgb555 numeric",
        16u,
        0u,
        0u,
        0u,
        0u,
        BMP_NUMERIC_V2_HEADER_SIZE,
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_fail_v2_alphabitfields_numeric_test(void)
{
    static unsigned char const payload[] = {
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };

    return run_builtin_loader_bmp_v2_fail_buffer_case(
        "builtin loader bmp fail v2 alphabitfields numeric",
        32u,
        6u,
        0x00ff0000u,
        0x0000ff00u,
        0x000000ffu,
        BMP_NUMERIC_V2_HEADER_SIZE,
        payload,
        sizeof(payload));
}

static int
run_builtin_loader_bmp_fail_v2_truncated_masks_numeric_test(void)
{
    static unsigned char const payload[] = {
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };

    return run_builtin_loader_bmp_v2_fail_buffer_case(
        "builtin loader bmp fail v2 truncated masks numeric",
        16u,
        3u,
        0x0000f800u,
        0x000007e0u,
        0x0000001fu,
        64u,
        payload,
        sizeof(payload));
}

static int
run_builtin_loader_bmp_fail_v2_zero_color_masks_numeric_test(void)
{
    static unsigned char const payload[] = {
        0x1fu, 0x00u, 0xffu, 0xffu, 0x00u, 0xf8u, 0xe0u, 0x07u
    };

    return run_builtin_loader_bmp_v2_fail_buffer_case(
        "builtin loader bmp fail v2 zero color masks numeric",
        16u,
        3u,
        0x00000000u,
        0x000007e0u,
        0x0000001fu,
        BMP_NUMERIC_V2_HEADER_SIZE,
        payload,
        sizeof(payload));
}

#define BMP_NUM_I40_A0_MAX_BMP_SIZE 256u
#define BMP_NUM_I40_A0_DIB_SIZE 40u
#define BMP_NUM_I40_A0_MASK_SIZE 16u
#define BMP_NUM_I40_A0_PIXEL_OFFSET \
    (14u + BMP_NUM_I40_A0_DIB_SIZE + BMP_NUM_I40_A0_MASK_SIZE)

static int
bmp_num_mk_i40_a0_bf(unsigned char *bmp,
                     size_t bmp_capacity,
                     unsigned int compression,
                     unsigned char const *payload,
                     size_t payload_size,
                     size_t *bmp_size)
{
    size_t file_size;
    uint32_t file_size_u32;

    file_size = 0u;
    file_size_u32 = 0u;
    if (bmp == NULL || payload == NULL || bmp_size == NULL) {
        return 0;
    }
    if (BMP_NUM_I40_A0_PIXEL_OFFSET > SIZE_MAX - payload_size) {
        return 0;
    }
    file_size = BMP_NUM_I40_A0_PIXEL_OFFSET + payload_size;
    if (file_size > bmp_capacity || file_size > 0xffffffffu ||
        payload_size > 0xffffffffu) {
        return 0;
    }

    file_size_u32 = (uint32_t)file_size;
    memset(bmp, 0, file_size);
    bmp[0] = 0x42u;
    bmp[1] = 0x4du;
    bmp_numeric_write_u32le(bmp, 2u, file_size_u32);
    bmp_numeric_write_u32le(bmp, 10u, (uint32_t)BMP_NUM_I40_A0_PIXEL_OFFSET);
    bmp_numeric_write_u32le(bmp, 14u, BMP_NUM_I40_A0_DIB_SIZE);
    bmp_numeric_write_u32le(bmp, 18u, 2u);
    bmp_numeric_write_u32le(bmp, 22u, 2u);
    bmp_numeric_write_u16le(bmp, 26u, 1u);
    bmp_numeric_write_u16le(bmp, 28u, 32u);
    bmp_numeric_write_u32le(bmp, 30u, (uint32_t)compression);
    bmp_numeric_write_u32le(bmp, 34u, (uint32_t)payload_size);
    bmp_numeric_write_u32le(bmp, 38u, 0x00000b13u);
    bmp_numeric_write_u32le(bmp, 42u, 0x00000b13u);
    bmp_numeric_write_u32le(bmp, 54u, 0x00ff0000u);
    bmp_numeric_write_u32le(bmp, 58u, 0x0000ff00u);
    bmp_numeric_write_u32le(bmp, 62u, 0x000000ffu);
    bmp_numeric_write_u32le(bmp, 66u, 0xff000000u);
    memcpy(bmp + BMP_NUM_I40_A0_PIXEL_OFFSET, payload, payload_size);
    *bmp_size = file_size;
    return 1;
}

static int
run_bmp_i40_a0_mask_case(char const *label, unsigned int compression)
{
    static unsigned char const payload[16] = {
        0x33u, 0x22u, 0x11u, 0x00u, 0x66u, 0x55u, 0x44u, 0x00u,
        0x99u, 0x88u, 0x77u, 0x00u, 0xccu, 0xbbu, 0xaau, 0x00u
    };
    static unsigned char const expected_rgb[12] = {
        0x77u, 0x88u, 0x99u, 0xaau, 0xbbu, 0xccu,
        0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u
    };
    static unsigned char const expected_mask[4] = {
        1u, 1u, 1u, 1u
    };
    unsigned char bmp[BMP_NUM_I40_A0_MAX_BMP_SIZE];
    builtin_loader_probe_options_t options;
    bmp_numeric_probe_context_t probe;
    SIXELSTATUS status;
    size_t bmp_size;
    int result;

    status = SIXEL_FALSE;
    memset(&options, 0, sizeof(options));
    memset(&probe, 0, sizeof(probe));
    memset(bmp, 0, sizeof(bmp));
    bmp_size = 0u;
    result = 1;

    if (!bmp_num_mk_i40_a0_bf(bmp,
                              sizeof(bmp),
                              compression,
                              payload,
                              sizeof(payload),
                              &bmp_size)) {
        fprintf(stderr, "%s: failed to build INFO40 all-zero alpha BMP\n",
                label);
        return 1;
    }

    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;

    result = run_builtin_loader_probe_buffer_case(label,
                                                  bmp,
                                                  bmp_size,
                                                  &options,
                                                  capture_bmp_numeric_probe,
                                                  &probe,
                                                  &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: loader failed (%d)\n", label, (int)status);
        return 1;
    }
    return verify_bmp_rgb_mask_probe(label,
                                     &probe,
                                     2,
                                     2,
                                     expected_rgb,
                                     sizeof(expected_rgb),
                                     expected_mask,
                                     sizeof(expected_mask));
}

static int
run_bmp_i40_abf_a0_mask_num_t(void)
{
    return run_bmp_i40_a0_mask_case(
        "builtin loader bmp info40 alphabitfields all-zero alpha "
        "mask no-bg numeric",
        6u);
}

static int
run_bmp_i40_bf_a0_mask_num_t(void)
{
    return run_bmp_i40_a0_mask_case(
        "builtin loader bmp info40 bitfields explicit all-zero alpha "
        "mask no-bg numeric",
        3u);
}

#define BMP_NUM_OS2S_MAX_BMP_SIZE 2048u

static int
bmp_num_build_os2s_bmp(unsigned char *bmp,
                       size_t bmp_cap,
                       unsigned int dib_size,
                       int width,
                       int height_signed,
                       unsigned int bpp,
                       unsigned int comp,
                       unsigned int image_size,
                       unsigned int colors_used,
                       unsigned char const *palette,
                       size_t palette_size,
                       unsigned char const *payload,
                       size_t payload_size,
                       size_t *bmp_size)
{
    size_t pixel_off;
    size_t file_size;
    size_t hdr_size;
    uint32_t file_u32;
    uint32_t off_u32;

    pixel_off = 0u;
    file_size = 0u;
    hdr_size = 0u;
    file_u32 = 0u;
    off_u32 = 0u;
    if (bmp == NULL || payload == NULL || bmp_size == NULL) {
        return 0;
    }
    if (width <= 0 || height_signed == 0) {
        return 0;
    }
    if (dib_size != 16u &&
        dib_size != 24u &&
        dib_size != 32u &&
        dib_size != 40u &&
        dib_size != 64u) {
        return 0;
    }
    hdr_size = 14u + (size_t)dib_size;
    if (hdr_size > SIZE_MAX - palette_size) {
        return 0;
    }
    pixel_off = hdr_size + palette_size;
    if (pixel_off > SIZE_MAX - payload_size) {
        return 0;
    }
    file_size = pixel_off + payload_size;
    if (file_size > bmp_cap || file_size > 0xffffffffu ||
        pixel_off > 0xffffffffu) {
        return 0;
    }

    file_u32 = (uint32_t)file_size;
    off_u32 = (uint32_t)pixel_off;
    memset(bmp, 0, file_size);
    bmp[0] = 0x42u;
    bmp[1] = 0x4du;
    bmp_numeric_write_u32le(bmp, 2u, file_u32);
    bmp_numeric_write_u32le(bmp, 10u, off_u32);
    bmp_numeric_write_u32le(bmp, 14u, dib_size);
    bmp_numeric_write_u32le(bmp, 18u, (uint32_t)(unsigned int)width);
    bmp_numeric_write_u32le(bmp, 22u, (uint32_t)height_signed);
    bmp_numeric_write_u16le(bmp, 26u, 1u);
    bmp_numeric_write_u16le(bmp, 28u, bpp);
    if (dib_size >= 20u) {
        bmp_numeric_write_u32le(bmp, 30u, comp);
    }
    if (dib_size >= 24u) {
        bmp_numeric_write_u32le(bmp, 34u, image_size);
    }
    if (dib_size >= 28u) {
        bmp_numeric_write_u32le(bmp, 38u, 0x00000b13u);
    }
    if (dib_size >= 32u) {
        bmp_numeric_write_u32le(bmp, 42u, 0x00000b13u);
    }
    if (dib_size >= 36u) {
        bmp_numeric_write_u32le(bmp, 46u, colors_used);
    }
    if (palette_size != 0u && palette != NULL) {
        memcpy(bmp + hdr_size, palette, palette_size);
    }
    memcpy(bmp + pixel_off, payload, payload_size);
    *bmp_size = file_size;
    return 1;
}

static int
run_bmp_os2s_rgb_case(char const *label,
                      unsigned int dib_size,
                      int width,
                      int height_signed,
                      unsigned int bpp,
                      unsigned int comp,
                      unsigned int image_size,
                      unsigned int colors_used,
                      unsigned char const *palette,
                      size_t palette_size,
                      unsigned char const *payload,
                      size_t payload_size,
                      unsigned char const *expected_rgb,
                      size_t expected_rgb_size)
{
    unsigned char bmp[BMP_NUM_OS2S_MAX_BMP_SIZE];
    size_t bmp_size;
    int height;

    bmp_size = 0u;
    height = 0;
    memset(bmp, 0, sizeof(bmp));
    if (!bmp_num_build_os2s_bmp(bmp,
                                sizeof(bmp),
                                dib_size,
                                width,
                                height_signed,
                                bpp,
                                comp,
                                image_size,
                                colors_used,
                                palette,
                                palette_size,
                                payload,
                                payload_size,
                                &bmp_size)) {
        fprintf(stderr, "%s: failed to build OS/2 short BMP\n", label);
        return 1;
    }
    height = height_signed < 0 ? -height_signed : height_signed;
    return run_builtin_loader_bmp_rgb_buffer_case(label,
                                                  bmp,
                                                  bmp_size,
                                                  width,
                                                  height,
                                                  expected_rgb,
                                                  expected_rgb_size);
}

static int
run_bmp_os2s_fail_case(char const *label,
                       unsigned int dib_size,
                       int width,
                       int height_signed,
                       unsigned int bpp,
                       unsigned int comp,
                       unsigned int image_size,
                       unsigned int colors_used,
                       unsigned char const *palette,
                       size_t palette_size,
                       unsigned char const *payload,
                       size_t payload_size)
{
    unsigned char bmp[BMP_NUM_OS2S_MAX_BMP_SIZE];
    size_t bmp_size;

    bmp_size = 0u;
    memset(bmp, 0, sizeof(bmp));
    if (!bmp_num_build_os2s_bmp(bmp,
                                sizeof(bmp),
                                dib_size,
                                width,
                                height_signed,
                                bpp,
                                comp,
                                image_size,
                                colors_used,
                                palette,
                                palette_size,
                                payload,
                                payload_size,
                                &bmp_size)) {
        fprintf(stderr, "%s: failed to build OS/2 short BMP\n", label);
        return 1;
    }
    return run_builtin_loader_bmp_expect_fail_buffer_case(label,
                                                          bmp,
                                                          bmp_size);
}

static int
run_bmp_os2s16_rgb24_num_t(void)
{
    static unsigned char const payload[] = {
        0xffu, 0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0xffu, 0xffu, 0xffu
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE", "") != 0) {
        return 1;
    }
    return run_bmp_os2s_rgb_case(
        "builtin loader bmp os2 short16 rgb24 numeric",
        16u,
        2,
        2,
        24u,
        0u,
        (unsigned int)sizeof(payload),
        0u,
        NULL,
        0u,
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_bmp_os2s24_rle8_num_t(void)
{
    static unsigned char const payload[] = {
        0x01u, 0x02u, 0x01u, 0x03u, 0x00u, 0x00u, 0x01u, 0x01u,
        0x01u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u, 0x00u, 0x00u, 0xffu
    };
    unsigned char palette[256u * 4u];

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE", "") != 0) {
        return 1;
    }
    memset(palette, 0, sizeof(palette));
    palette[4u + 2u] = 0xffu;
    palette[8u + 1u] = 0xffu;
    palette[12u + 0u] = 0xffu;
    return run_bmp_os2s_rgb_case(
        "builtin loader bmp os2 short24 rle8 numeric",
        24u,
        2,
        2,
        8u,
        1u,
        (unsigned int)sizeof(payload),
        0u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_bmp_os2s32_rle4_num_t(void)
{
    static unsigned char const payload[] = {
        0x02u, 0x23u, 0x00u, 0x00u, 0x02u, 0x10u, 0x00u, 0x00u,
        0x00u, 0x01u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0xffu, 0x00u, 0x00u, 0x00u, 0xffu
    };
    unsigned char palette[16u * 4u];

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE", "") != 0) {
        return 1;
    }
    memset(palette, 0, sizeof(palette));
    palette[4u + 2u] = 0xffu;
    palette[8u + 1u] = 0xffu;
    palette[12u + 0u] = 0xffu;
    return run_bmp_os2s_rgb_case(
        "builtin loader bmp os2 short32 rle4 numeric",
        32u,
        2,
        2,
        4u,
        2u,
        (unsigned int)sizeof(payload),
        0u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_bmp_i40_auto_huff_num_t(void)
{
    static unsigned char const palette[] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const payload[] = {
        0x35u, 0xc0u, 0x04u, 0x74u, 0x00u, 0x20u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
                           "auto") != 0) {
        return 1;
    }
    return run_bmp_os2s_rgb_case(
        "builtin loader bmp info40 auto huffman1d numeric",
        40u,
        2,
        2,
        1u,
        3u,
        (unsigned int)sizeof(payload),
        2u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_bmp_i40_2bpp_pal_num_t(void)
{
    static unsigned char const palette[] = {
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0xffu, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const payload[] = {
        0xc0u, 0x00u, 0x00u, 0x00u,
        0x60u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE", "") != 0) {
        return 1;
    }
    return run_bmp_os2s_rgb_case(
        "builtin loader bmp info40 2bpp palette numeric",
        40u,
        2,
        2,
        2u,
        0u,
        (unsigned int)sizeof(payload),
        0u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_bmp_i40_2bpp_pal_topdown_num_t(void)
{
    static unsigned char const palette[] = {
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0xffu, 0x00u, 0x00u,
        0xffu, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const payload[] = {
        0x60u, 0x00u, 0x00u, 0x00u,
        0xc0u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE", "") != 0) {
        return 1;
    }
    return run_bmp_os2s_rgb_case(
        "builtin loader bmp info40 2bpp palette topdown numeric",
        40u,
        2,
        -2,
        2u,
        0u,
        (unsigned int)sizeof(payload),
        0u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_bmp_i40_fail_2bpp_pal_ovf_t(void)
{
    static unsigned char const palette[] = {
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u
    };
    static unsigned char const payload[] = {
        0xc0u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE", "") != 0) {
        return 1;
    }
    return run_bmp_os2s_fail_case(
        "builtin loader bmp fail info40 2bpp palette overflow numeric",
        40u,
        2,
        2,
        2u,
        0u,
        (unsigned int)sizeof(payload),
        2u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload));
}

static int
run_bmp_i40_win_huff_fail_t(void)
{
    static unsigned char const palette[] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const payload[] = {
        0x35u, 0xc0u, 0x04u, 0x74u, 0x00u, 0x20u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
                           "windows") != 0) {
        return 1;
    }
    return run_bmp_os2s_fail_case(
        "builtin loader bmp info40 windows huffman1d fail numeric",
        40u,
        2,
        2,
        1u,
        3u,
        (unsigned int)sizeof(payload),
        2u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload));
}

static int
run_bmp_i40_os2_huff_num_t(void)
{
    static unsigned char const palette[] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const payload[] = {
        0x35u, 0xc0u, 0x04u, 0x74u, 0x00u, 0x20u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
                           "os2") != 0) {
        return 1;
    }
    return run_bmp_os2s_rgb_case(
        "builtin loader bmp info40 os2 huffman1d numeric",
        40u,
        2,
        2,
        1u,
        3u,
        (unsigned int)sizeof(payload),
        2u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_bmp_i40_auto_r24_num_t(void)
{
    static unsigned char const payload[] = {
        0x01u, 0xffu, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0xffu, 0x01u, 0x00u,
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u
    };
    static unsigned char const expected_rgb[12] = {
        0xffu, 0x00u, 0x00u, 0x00u, 0xffu, 0x00u,
        0x00u, 0x00u, 0xffu, 0x00u, 0x00u, 0x00u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
                           "auto") != 0) {
        return 1;
    }
    return run_bmp_os2s_rgb_case(
        "builtin loader bmp info40 auto rle24 numeric",
        40u,
        2,
        2,
        24u,
        4u,
        (unsigned int)sizeof(payload),
        0u,
        NULL,
        0u,
        payload,
        sizeof(payload),
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_bmp_i40_win_r24_fail_t(void)
{
    static unsigned char const payload[] = {
        0x01u, 0xffu, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0xffu, 0x01u, 0x00u,
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
                           "windows") != 0) {
        return 1;
    }
    return run_bmp_os2s_fail_case(
        "builtin loader bmp info40 windows rle24 fail numeric",
        40u,
        2,
        2,
        24u,
        4u,
        (unsigned int)sizeof(payload),
        0u,
        NULL,
        0u,
        payload,
        sizeof(payload));
}

static int
run_bmp_i40_win_c14_fail_t(void)
{
    static unsigned char const palette[] = {
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const payload[] = {
        0x35u, 0xc0u, 0x04u, 0x74u, 0x00u, 0x20u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
                           "windows") != 0) {
        return 1;
    }
    return run_bmp_os2s_fail_case(
        "builtin loader bmp info40 windows comp14 fail numeric",
        40u,
        2,
        2,
        1u,
        14u,
        (unsigned int)sizeof(payload),
        2u,
        palette,
        sizeof(palette),
        payload,
        sizeof(payload));
}

static int
run_bmp_i40_win_c15_fail_t(void)
{
    static unsigned char const payload[] = {
        0x01u, 0xffu, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0xffu, 0x01u, 0x00u,
        0xffu, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u
    };

    if (loader_test_setenv("SIXEL_LOADER_BUILTIN_BMP_INFO40_MODE",
                           "windows") != 0) {
        return 1;
    }
    return run_bmp_os2s_fail_case(
        "builtin loader bmp info40 windows comp15 fail numeric",
        40u,
        2,
        2,
        24u,
        15u,
        (unsigned int)sizeof(payload),
        0u,
        NULL,
        0u,
        payload,
        sizeof(payload));
}

static int
bmp_num_build_i40_hdr(unsigned char *bmp,
                      size_t bmp_capacity,
                      int width,
                      int height_signed,
                      unsigned int bpp,
                      unsigned int compression,
                      unsigned int image_size,
                      unsigned int pixel_offset)
{
    unsigned int file_size;

    file_size = 54u;
    if (bmp == NULL || bmp_capacity < (size_t)file_size) {
        return 0;
    }

    memset(bmp, 0, (size_t)file_size);
    bmp[0] = 0x42u;
    bmp[1] = 0x4du;
    bmp_numeric_write_u32le(bmp, 2u, file_size);
    bmp_numeric_write_u32le(bmp, 10u, pixel_offset);
    bmp_numeric_write_u32le(bmp, 14u, 40u);
    bmp_numeric_write_u32le(bmp, 18u, (uint32_t)(unsigned int)width);
    bmp_numeric_write_u32le(bmp, 22u, (uint32_t)height_signed);
    bmp_numeric_write_u16le(bmp, 26u, 1u);
    bmp_numeric_write_u16le(bmp, 28u, bpp);
    bmp_numeric_write_u32le(bmp, 30u, compression);
    bmp_numeric_write_u32le(bmp, 34u, image_size);
    bmp_numeric_write_u32le(bmp, 38u, 0x00000b13u);
    bmp_numeric_write_u32le(bmp, 42u, 0x00000b13u);

    return 1;
}

static int
run_bmp_i40_fail_rgb32_imgovf_t(void)
{
    unsigned char bmp[54u];

    memset(bmp, 0, sizeof(bmp));
    if (!bmp_num_build_i40_hdr(bmp,
                               sizeof(bmp),
                               2147483647,
                               2147483647,
                               32u,
                               0u,
                               0u,
                               54u)) {
        fprintf(stderr,
                "builtin loader bmp fail info40 rgb32 image overflow "
                "numeric: failed to build buffer\n");
        return 1;
    }
    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail info40 rgb32 image overflow numeric",
        bmp,
        sizeof(bmp));
}

static int
run_bmp_i40_fail_pixoff_eqsize_t(void)
{
    unsigned char bmp[54u];

    memset(bmp, 0, sizeof(bmp));
    if (!bmp_num_build_i40_hdr(bmp,
                               sizeof(bmp),
                               2,
                               2,
                               24u,
                               0u,
                               0u,
                               54u)) {
        fprintf(stderr,
                "builtin loader bmp fail info40 pixel offset equal file "
                "size numeric: failed to build buffer\n");
        return 1;
    }
    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail info40 pixel offset equal file size "
        "numeric",
        bmp,
        sizeof(bmp));
}

static int
run_bmp_i40_fail_paloff_t(void)
{
    unsigned char bmp[54u];

    memset(bmp, 0, sizeof(bmp));
    if (!bmp_num_build_i40_hdr(bmp,
                               sizeof(bmp),
                               2,
                               2,
                               8u,
                               0u,
                               0u,
                               53u)) {
        fprintf(stderr,
                "builtin loader bmp fail info40 palette offset numeric: "
                "failed to build buffer\n");
        return 1;
    }
    return run_builtin_loader_bmp_expect_fail_buffer_case(
        "builtin loader bmp fail info40 palette offset numeric",
        bmp,
        sizeof(bmp));
}

static int
bmp_numeric_append_huffman_bits(unsigned char *payload,
                                size_t payload_capacity,
                                size_t *bit_count,
                                char const *bits)
{
    size_t index;
    size_t byte_index;
    unsigned int bit_index;

    index = 0u;
    byte_index = 0u;
    bit_index = 0u;
    if (payload == NULL || bit_count == NULL || bits == NULL) {
        return 0;
    }

    for (index = 0u; bits[index] != '\0'; ++index) {
        if (bits[index] != '0' && bits[index] != '1') {
            return 0;
        }
        byte_index = *bit_count >> 3u;
        if (byte_index >= payload_capacity) {
            return 0;
        }
        bit_index = (unsigned int)(7u - (*bit_count & 7u));
        if (bits[index] == '1') {
            payload[byte_index] = (unsigned char)(payload[byte_index]
                | (unsigned char)(1u << bit_index));
        }
        *bit_count += 1u;
    }

    return 1;
}

static int
bmp_numeric_append_huffman_zero_fill(unsigned char *payload,
                                     size_t payload_capacity,
                                     size_t *bit_count,
                                     unsigned int zero_bits)
{
    unsigned int index;

    index = 0u;
    if (payload == NULL || bit_count == NULL) {
        return 0;
    }

    for (index = 0u; index < zero_bits; ++index) {
        if (!bmp_numeric_append_huffman_bits(payload,
                                             payload_capacity,
                                             bit_count,
                                             "0")) {
            return 0;
        }
    }
    return 1;
}

static int
bmp_numeric_append_huffman_eol(unsigned char *payload,
                               size_t payload_capacity,
                               size_t *bit_count,
                               unsigned int fill_zeros)
{
    if (!bmp_numeric_append_huffman_zero_fill(payload,
                                              payload_capacity,
                                              bit_count,
                                              fill_zeros)) {
        return 0;
    }
    return bmp_numeric_append_huffman_bits(payload,
                                           payload_capacity,
                                           bit_count,
                                           "000000000001");
}

static int
bmp_numeric_build_os2_huffman1d_bmp(unsigned char *bmp,
                                    size_t bmp_capacity,
                                    int width,
                                    int height_signed,
                                    unsigned char const *payload,
                                    size_t payload_size,
                                    size_t *bmp_size)
{
    size_t file_size;
    uint32_t width_u32;
    uint32_t height_u32;
    uint32_t payload_u32;
    uint32_t file_size_u32;

    file_size = 0u;
    width_u32 = 0u;
    height_u32 = 0u;
    payload_u32 = 0u;
    file_size_u32 = 0u;
    if (bmp == NULL || payload == NULL || bmp_size == NULL) {
        return 0;
    }
    if (width <= 0 || height_signed == 0) {
        return 0;
    }
    if (payload_size > 0xffffffffu) {
        return 0;
    }
    if (BMP_NUMERIC_OS2_HUFFMAN1D_PIXEL_OFFSET >
        SIZE_MAX - payload_size) {
        return 0;
    }
    file_size = BMP_NUMERIC_OS2_HUFFMAN1D_PIXEL_OFFSET + payload_size;
    if (file_size > bmp_capacity || file_size > 0xffffffffu) {
        return 0;
    }

    width_u32 = (uint32_t)(unsigned int)width;
    height_u32 = (uint32_t)height_signed;
    payload_u32 = (uint32_t)payload_size;
    file_size_u32 = (uint32_t)file_size;

    memset(bmp, 0, file_size);
    bmp[0] = 0x42u;
    bmp[1] = 0x4du;
    bmp_numeric_write_u32le(bmp, 2u, file_size_u32);
    bmp_numeric_write_u32le(bmp, 10u,
                            (uint32_t)BMP_NUMERIC_OS2_HUFFMAN1D_PIXEL_OFFSET);
    bmp_numeric_write_u32le(bmp, 14u, 64u);
    bmp_numeric_write_u32le(bmp, 18u, width_u32);
    bmp_numeric_write_u32le(bmp, 22u, height_u32);
    bmp_numeric_write_u16le(bmp, 26u, 1u);
    bmp_numeric_write_u16le(bmp, 28u, 1u);
    bmp_numeric_write_u32le(bmp, 30u, 3u);
    bmp_numeric_write_u32le(bmp, 34u, payload_u32);
    bmp_numeric_write_u32le(bmp, 38u, 0x00000b13u);
    bmp_numeric_write_u32le(bmp, 42u, 0x00000b13u);
    bmp_numeric_write_u32le(bmp, 46u, 2u);
    bmp[78u] = 0xffu;
    bmp[79u] = 0xffu;
    bmp[80u] = 0xffu;
    bmp[81u] = 0x00u;
    bmp[82u] = 0x00u;
    bmp[83u] = 0x00u;
    bmp[84u] = 0x00u;
    bmp[85u] = 0x00u;
    memcpy(bmp + BMP_NUMERIC_OS2_HUFFMAN1D_PIXEL_OFFSET,
           payload,
           payload_size);
    *bmp_size = file_size;

    return 1;
}

static int
run_builtin_loader_bmp_os2_huffman1d_generated_rgb_case(
    char const *label,
    int width,
    int height_signed,
    unsigned char const *payload,
    size_t payload_size,
    unsigned char const *expected_rgb,
    size_t expected_rgb_size)
{
    unsigned char bmp[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_BMP_SIZE];
    size_t bmp_size;
    int expected_height;

    bmp_size = 0u;
    expected_height = 0;
    memset(bmp, 0, sizeof(bmp));
    if (!bmp_numeric_build_os2_huffman1d_bmp(bmp,
                                             sizeof(bmp),
                                             width,
                                             height_signed,
                                             payload,
                                             payload_size,
                                             &bmp_size)) {
        fprintf(stderr, "%s: failed to build OS/2 HUFFMAN1D buffer\n", label);
        return 1;
    }

    expected_height = height_signed < 0 ? -height_signed : height_signed;
    return run_builtin_loader_bmp_rgb_buffer_case(label,
                                                  bmp,
                                                  bmp_size,
                                                  width,
                                                  expected_height,
                                                  expected_rgb,
                                                  expected_rgb_size);
}

static int
run_builtin_loader_bmp_os2_huffman1d_generated_fail_case(
    char const *label,
    int width,
    int height_signed,
    unsigned char const *payload,
    size_t payload_size)
{
    unsigned char bmp[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_BMP_SIZE];
    size_t bmp_size;

    bmp_size = 0u;
    memset(bmp, 0, sizeof(bmp));
    if (!bmp_numeric_build_os2_huffman1d_bmp(bmp,
                                             sizeof(bmp),
                                             width,
                                             height_signed,
                                             payload,
                                             payload_size,
                                             &bmp_size)) {
        fprintf(stderr, "%s: failed to build OS/2 HUFFMAN1D buffer\n", label);
        return 1;
    }
    return run_builtin_loader_bmp_expect_fail_buffer_case(label,
                                                          bmp,
                                                          bmp_size);
}

static int
run_builtin_loader_bmp_os2_huffman1d_long_run_numeric_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    unsigned char expected_rgb[12];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    memset(expected_rgb, 0, sizeof(expected_rgb));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "00110101") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "0000001111") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "11") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "1011") ||
        !bmp_numeric_append_huffman_eol(payload,
                                        sizeof(payload),
                                        &bit_count,
                                        0u)) {
        fprintf(stderr,
                "builtin loader bmp os2 huffman1d long run numeric: "
                "failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;

    return run_builtin_loader_bmp_os2_huffman1d_generated_rgb_case(
        "builtin loader bmp os2 huffman1d long run numeric",
        70,
        1,
        payload,
        payload_size,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_os2_huffman1d_makeup_chain_numeric_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    unsigned char expected_rgb[12];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    memset(expected_rgb, 0, sizeof(expected_rgb));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "00110101") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "0000001111") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "0000001111") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "0000110111") ||
        !bmp_numeric_append_huffman_eol(payload,
                                        sizeof(payload),
                                        &bit_count,
                                        0u)) {
        fprintf(stderr,
                "builtin loader bmp os2 huffman1d makeup chain numeric: "
                "failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;

    return run_builtin_loader_bmp_os2_huffman1d_generated_rgb_case(
        "builtin loader bmp os2 huffman1d makeup chain numeric",
        128,
        1,
        payload,
        payload_size,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_os2_huffman1d_boundary_128_numeric_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    unsigned char expected_rgb[12];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    memset(expected_rgb, 0, sizeof(expected_rgb));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "00110101") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "000011001000") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "0000110111") ||
        !bmp_numeric_append_huffman_eol(payload,
                                        sizeof(payload),
                                        &bit_count,
                                        0u)) {
        fprintf(stderr,
                "builtin loader bmp os2 huffman1d boundary 128 numeric: "
                "failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;

    return run_builtin_loader_bmp_os2_huffman1d_generated_rgb_case(
        "builtin loader bmp os2 huffman1d boundary 128 numeric",
        128,
        1,
        payload,
        payload_size,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_os2_huff1d_multiline_fill_num_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    unsigned char expected_rgb[12];
    size_t bit_count;
    size_t payload_size;
    size_t index;

    bit_count = 0u;
    payload_size = 0u;
    index = 0u;
    memset(payload, 0, sizeof(payload));
    memset(expected_rgb, 0, sizeof(expected_rgb));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "00110101") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "000101") ||
        !bmp_numeric_append_huffman_eol(payload,
                                        sizeof(payload),
                                        &bit_count,
                                        3u) ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "10011") ||
        !bmp_numeric_append_huffman_eol(payload,
                                        sizeof(payload),
                                        &bit_count,
                                        5u)) {
        fprintf(stderr,
                "builtin loader bmp os2 huffman1d multiline fill numeric: "
                "failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;
    for (index = 0u; index < 4u; ++index) {
        expected_rgb[index * 3u + 0u] = 0xffu;
        expected_rgb[index * 3u + 1u] = 0xffu;
        expected_rgb[index * 3u + 2u] = 0xffu;
    }

    return run_builtin_loader_bmp_os2_huffman1d_generated_rgb_case(
        "builtin loader bmp os2 huffman1d multiline fill numeric",
        8,
        2,
        payload,
        payload_size,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_os2_huff1d_short_compat_num_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    unsigned char expected_rgb[12];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    memset(expected_rgb, 0, sizeof(expected_rgb));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "00110101") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "11") ||
        !bmp_numeric_append_huffman_eol(payload,
                                        sizeof(payload),
                                        &bit_count,
                                        0u) ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "000111") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "010") ||
        !bmp_numeric_append_huffman_eol(payload,
                                        sizeof(payload),
                                        &bit_count,
                                        0u)) {
        fprintf(stderr,
                "builtin loader bmp os2 huffman1d short run compat numeric: "
                "failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;
    expected_rgb[0u] = 0xffu;
    expected_rgb[1u] = 0xffu;
    expected_rgb[2u] = 0xffu;

    return run_builtin_loader_bmp_os2_huffman1d_generated_rgb_case(
        "builtin loader bmp os2 huffman1d short run compat numeric",
        2,
        2,
        payload,
        payload_size,
        expected_rgb,
        sizeof(expected_rgb));
}

static int
run_builtin_loader_bmp_fail_os2_huff1d_missing_eol_num_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "10011")) {
        fprintf(stderr,
                "builtin loader bmp fail os2 huffman1d missing eol numeric: "
                "failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;
    return run_builtin_loader_bmp_os2_huffman1d_generated_fail_case(
        "builtin loader bmp fail os2 huffman1d missing eol numeric",
        8,
        1,
        payload,
        payload_size);
}

static int
run_builtin_loader_bmp_fail_os2_huff1d_row_overflow_num_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "00110101") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "000100") ||
        !bmp_numeric_append_huffman_eol(payload,
                                        sizeof(payload),
                                        &bit_count,
                                        0u)) {
        fprintf(stderr,
                "builtin loader bmp fail os2 huffman1d row overflow numeric: "
                "failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;
    return run_builtin_loader_bmp_os2_huffman1d_generated_fail_case(
        "builtin loader bmp fail os2 huffman1d row overflow numeric",
        8,
        1,
        payload,
        payload_size);
}

static int
run_builtin_loader_bmp_fail_os2_huff1d_trunc_makeup_num_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "110110")) {
        fprintf(stderr,
                "builtin loader bmp fail os2 huffman1d truncated makeup "
                "numeric: failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;
    return run_builtin_loader_bmp_os2_huffman1d_generated_fail_case(
        "builtin loader bmp fail os2 huffman1d truncated makeup numeric",
        128,
        1,
        payload,
        payload_size);
}

static int
run_builtin_loader_bmp_fail_os2_huff1d_invalid_eol_num_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "10011") ||
        !bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "1")) {
        fprintf(stderr,
                "builtin loader bmp fail os2 huffman1d invalid eol numeric: "
                "failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;
    return run_builtin_loader_bmp_os2_huffman1d_generated_fail_case(
        "builtin loader bmp fail os2 huffman1d invalid eol numeric",
        8,
        1,
        payload,
        payload_size);
}

static int
run_builtin_loader_bmp_fail_os2_huff1d_invalid_code2_num_test(void)
{
    unsigned char payload[BMP_NUMERIC_OS2_HUFFMAN1D_MAX_PAYLOAD];
    size_t bit_count;
    size_t payload_size;

    bit_count = 0u;
    payload_size = 0u;
    memset(payload, 0, sizeof(payload));
    if (!bmp_numeric_append_huffman_bits(payload,
                                         sizeof(payload),
                                         &bit_count,
                                         "000000000000")) {
        fprintf(stderr,
                "builtin loader bmp fail os2 huffman1d invalid code2 "
                "numeric: failed to build payload\n");
        return 1;
    }
    payload_size = (bit_count + 7u) >> 3u;
    return run_builtin_loader_bmp_os2_huffman1d_generated_fail_case(
        "builtin loader bmp fail os2 huffman1d invalid code2 numeric",
        8,
        1,
        payload,
        payload_size);
}
