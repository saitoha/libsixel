static int
run_builtin_loader_hdr_numeric_probe_case(char const *label,
                                          char const *relative_path,
                                          int cms_engine,
                                          hdr_numeric_probe_context_t *context)
{
    SIXELSTATUS status;
    builtin_loader_probe_options_t options;
    int result;

    status = SIXEL_FALSE;
    result = 1;

    if (context == NULL) {
        fprintf(stderr, "%s: context is null\n", label);
        return 1;
    }

    builtin_loader_probe_options_init(&options);
    memset(context, 0, sizeof(*context));
    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = cms_engine;
    result = run_builtin_loader_probe_case(label,
                                           relative_path,
                                           &options,
                                           capture_hdr_numeric_probe,
                                           context,
                                           &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "%s: loader reported failure (%d)\n",
                label,
                (int)status);
        return 1;
    }

    if (context->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch\n", label);
        return 1;
    }
    if (context->width <= 0 || context->height <= 0) {
        fprintf(stderr,
                "%s: invalid geometry %dx%d\n",
                label,
                context->width,
                context->height);
        return 1;
    }

    return 0;
}

typedef enum hdr_numeric_case_validation_mode {
    HDR_NUMERIC_CASE_VALIDATE_EXACT = 0,
    HDR_NUMERIC_CASE_VALIDATE_CLAMP_RANGE
} hdr_numeric_case_validation_mode_t;

typedef struct hdr_numeric_static_case_spec {
    char const *label;
    char const *sample_path;
    int cms_engine;
    char const *fallback_profile;
    char const *exposure_ev;
    char const *tonemap;
    char const *use_header_exposure;
    int expected_pixelformat;
    int expected_colorspace;
    float expected_first_pixel[3];
    float tolerance;
    hdr_numeric_case_validation_mode_t validation_mode;
    float lower_bound;
    float upper_bound;
    int require_channel0_less_than;
    float channel0_less_than;
} hdr_numeric_static_case_spec_t;

typedef enum hdr_numeric_static_case_id {
    HDR_NUMERIC_STATIC_CASE_EXPOSURE = 0,
    HDR_NUMERIC_STATIC_CASE_TONEMAP_REINHARD,
    HDR_NUMERIC_STATIC_CASE_HEADER_EXPOSURE,
    HDR_NUMERIC_STATIC_CASE_HEADER_EXPOSURE_MULTI,
    HDR_NUMERIC_STATIC_CASE_HEADER_EXPOSURE_DISABLED,
    HDR_NUMERIC_STATIC_CASE_XYZE_FORMAT,
    HDR_NUMERIC_STATIC_CASE_COLORCORR_SINGLE,
    HDR_NUMERIC_STATIC_CASE_COLORCORR_MULTI,
    HDR_NUMERIC_STATIC_CASE_EXPOSURE_OVERFLOW_NONE,
    HDR_NUMERIC_STATIC_CASE_EXPOSURE_OVERFLOW_REINHARD
} hdr_numeric_static_case_id_t;

static hdr_numeric_static_case_spec_t const hdr_numeric_static_cases[] = {
    {
        "builtin loader hdr exposure numeric",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "1",
        "none",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 1.0f, 0.5f, 0.25f },
        0.0007f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        0,
        0.0f
    },
    {
        "builtin loader hdr tonemap numeric",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "0",
        "reinhard",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.33333334f, 0.2f, 0.11111111f },
        0.0007f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        1,
        0.5f
    },
    {
        "builtin loader hdr header exposure numeric",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "0",
        "none",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.25f, 0.125f, 0.0625f },
        0.0007f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        0,
        0.0f
    },
    {
        "builtin loader hdr header exposure multi numeric",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2x4.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "0",
        "none",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.0625f, 0.03125f, 0.015625f },
        0.0007f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        0,
        0.0f
    },
    {
        "builtin loader hdr header exposure disabled numeric",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "0",
        "none",
        "0",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.5f, 0.25f, 0.125f },
        0.0007f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        0,
        0.0f
    },
    {
        "builtin loader hdr xyze numeric",
        "/tests/data/inputs/formats/stbi_midtones_xyze.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "0",
        "none",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 1.1738129f, 0.0f, 0.10894224f },
        0.0012f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        0,
        0.0f
    },
    {
        "builtin loader hdr colorcorr single numeric",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_colorcorr_2_4_8.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "0",
        "none",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.25f, 0.0625f, 0.015625f },
        0.0007f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        0,
        0.0f
    },
    {
        "builtin loader hdr colorcorr multi numeric",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_colorcorr_2x4x8_multiline.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "0",
        "none",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.25f, 0.0625f, 0.015625f },
        0.0007f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        0,
        0.0f
    },
    {
        "builtin hdr exposure overflow none",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "200",
        "none",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.0f, 0.0f, 0.0f },
        0.0f,
        HDR_NUMERIC_CASE_VALIDATE_CLAMP_RANGE,
        FLT_MAX * 0.99f,
        FLT_MAX,
        0,
        0.0f
    },
    {
        "builtin hdr exposure overflow reinhard",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "linear-srgb",
        "200",
        "reinhard",
        "1",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 1.0f, 1.0f, 1.0f },
        0.0007f,
        HDR_NUMERIC_CASE_VALIDATE_EXACT,
        0.0f,
        0.0f,
        0,
        0.0f
    }
};

static int
run_builtin_loader_hdr_static_numeric_case(
    hdr_numeric_static_case_spec_t const *spec)
{
    hdr_numeric_probe_context_t probe;
    int index;
    int result;

    probe = (hdr_numeric_probe_context_t){ 0 };
    index = 0;
    result = 1;
    if (spec == NULL) {
        return 1;
    }

    if (hdr_test_configure_loader_env(spec->fallback_profile,
                                      spec->exposure_ev,
                                      spec->tonemap,
                                      spec->use_header_exposure) != 0) {
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        spec->label,
        spec->sample_path,
        spec->cms_engine,
        &probe);
    if (result != 0) {
        return result;
    }
    if (probe.pixelformat != spec->expected_pixelformat ||
        probe.colorspace != spec->expected_colorspace) {
        fprintf(stderr,
                "%s: frame contract mismatch (pf=%d expected=%d cs=%d "
                "expected=%d)\n",
                spec->label,
                probe.pixelformat,
                spec->expected_pixelformat,
                probe.colorspace,
                spec->expected_colorspace);
        return 1;
    }

    if (spec->validation_mode == HDR_NUMERIC_CASE_VALIDATE_CLAMP_RANGE) {
        for (index = 0; index < 3; ++index) {
            if (!isfinite((double)probe.first_pixel[index]) ||
                probe.first_pixel[index] < spec->lower_bound ||
                probe.first_pixel[index] > spec->upper_bound) {
                fprintf(stderr,
                        "%s: channel %d is outside clamp range "
                        "(actual=%f min=%f max=%f)\n",
                        spec->label,
                        index,
                        probe.first_pixel[index],
                        spec->lower_bound,
                        spec->upper_bound);
                return 1;
            }
        }
    } else {
        for (index = 0; index < 3; ++index) {
            if (!float_approx_equal(probe.first_pixel[index],
                                    spec->expected_first_pixel[index],
                                    spec->tolerance)) {
                fprintf(stderr,
                        "%s: channel %d mismatch (actual=%f expected=%f)\n",
                        spec->label,
                        index,
                        probe.first_pixel[index],
                        spec->expected_first_pixel[index]);
                return 1;
            }
        }
    }

    if (spec->require_channel0_less_than != 0 &&
        !(probe.first_pixel[0] < spec->channel0_less_than)) {
        fprintf(stderr,
                "%s: channel 0 threshold check failed (actual=%f limit=%f)\n",
                spec->label,
                probe.first_pixel[0],
                spec->channel0_less_than);
        return 1;
    }

    return 0;
}

typedef struct hdr_orientation_case_spec {
    char const *label;
    char const *sample_path;
} hdr_orientation_case_spec_t;

typedef struct hdr_orientation_probe_context {
    int callback_count;
    int pixelformat;
    int colorspace;
    int width;
    int height;
    float pixels[12];
} hdr_orientation_probe_context_t;

static hdr_orientation_case_spec_t const hdr_orientation_cases[] = {
    {
        "builtin hdr orientation -Y +X",
        "/tests/data/inputs/formats/stbi_corner2x2_orient_mY_pX.hdr"
    },
    {
        "builtin hdr orientation -Y -X",
        "/tests/data/inputs/formats/stbi_corner2x2_orient_mY_mX.hdr"
    },
    {
        "builtin hdr orientation +Y +X",
        "/tests/data/inputs/formats/stbi_corner2x2_orient_pY_pX.hdr"
    },
    {
        "builtin hdr orientation +Y -X",
        "/tests/data/inputs/formats/stbi_corner2x2_orient_pY_mX.hdr"
    },
    {
        "builtin hdr orientation +X +Y",
        "/tests/data/inputs/formats/stbi_corner2x2_orient_pX_pY.hdr"
    },
    {
        "builtin hdr orientation +X -Y",
        "/tests/data/inputs/formats/stbi_corner2x2_orient_pX_mY.hdr"
    },
    {
        "builtin hdr orientation -X +Y",
        "/tests/data/inputs/formats/stbi_corner2x2_orient_mX_pY.hdr"
    },
    {
        "builtin hdr orientation -X -Y",
        "/tests/data/inputs/formats/stbi_corner2x2_orient_mX_mY.hdr"
    }
};

static SIXELSTATUS
capture_hdr_orientation_probe(sixel_frame_t *frame, void *data)
{
    hdr_orientation_probe_context_t *context;
    float const *pixels;
    sixel_frame_pixels_view_t view;
    size_t sample_count;
    size_t index;
    SIXELSTATUS status;

    context = (hdr_orientation_probe_context_t *)data;
    pixels = NULL;
    memset(&view, 0, sizeof(view));
    sample_count = 0u;
    index = 0u;
    status = SIXEL_FALSE;
    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->colorspace = sixel_frame_get_colorspace(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);
    for (index = 0u;
         index < sizeof(context->pixels) / sizeof(context->pixels[0]);
         ++index) {
        context->pixels[index] = 0.0f;
    }

    if (!SIXEL_PIXELFORMAT_IS_FLOAT32(context->pixelformat) ||
        context->width <= 0 ||
        context->height <= 0) {
        return SIXEL_OK;
    }

    status = loader_test_frame_get_pixels_view(frame, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    pixels = view.pixels_float32;
    if (pixels == NULL) {
        return SIXEL_OK;
    }
    if (context->width > 2 || context->height > 2) {
        return SIXEL_OK;
    }
    sample_count = (size_t)context->width * (size_t)context->height * 3u;
    if (sample_count > sizeof(context->pixels) / sizeof(context->pixels[0])) {
        sample_count = sizeof(context->pixels) / sizeof(context->pixels[0]);
    }

    for (index = 0u; index < sample_count; ++index) {
        context->pixels[index] = pixels[index];
    }

    return SIXEL_OK;
}

static int
run_builtin_loader_hdr_orientation_probe_case(
    hdr_orientation_case_spec_t const *spec,
    hdr_orientation_probe_context_t *context)
{
    SIXELSTATUS status;
    builtin_loader_probe_options_t options;
    int result;

    status = SIXEL_FALSE;
    result = 1;
    builtin_loader_probe_options_init(&options);
    if (spec == NULL || context == NULL) {
        return 1;
    }

    memset(context, 0, sizeof(*context));
    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
    options.set_bgcolor = 0;
    options.bgcolor = NULL;
    options.set_loop_control = 0;
    options.loop_control = SIXEL_LOOP_AUTO;
    options.set_cms_engine = 1;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;
    result = run_builtin_loader_probe_case(spec->label,
                                           spec->sample_path,
                                           &options,
                                           capture_hdr_orientation_probe,
                                           context,
                                           &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "%s: loader reported failure (%d)\n",
                spec->label,
                (int)status);
        return 1;
    }
    if (context->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch\n", spec->label);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_hdr_orientation_numeric_test(void)
{
    static float const expected_pixels[12] = {
        0.5f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f,
        0.0f, 0.0f, 0.5f,
        0.5f, 0.5f, 0.5f
    };
    hdr_orientation_probe_context_t probe;
    size_t case_index;
    size_t sample_index;
    float tolerance;
    int result;

    probe = (hdr_orientation_probe_context_t){ 0 };
    case_index = 0u;
    sample_index = 0u;
    tolerance = 0.0007f;
    result = 1;
    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }

    for (case_index = 0u;
         case_index < sizeof(hdr_orientation_cases) /
                      sizeof(hdr_orientation_cases[0]);
         ++case_index) {
        result = run_builtin_loader_hdr_orientation_probe_case(
            &hdr_orientation_cases[case_index],
            &probe);
        if (result != 0) {
            return result;
        }
        if (probe.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
            probe.colorspace != SIXEL_COLORSPACE_LINEAR) {
            fprintf(stderr,
                    "%s: frame contract mismatch (pf=%d cs=%d)\n",
                    hdr_orientation_cases[case_index].label,
                    probe.pixelformat,
                    probe.colorspace);
            return 1;
        }
        if (probe.width != 2 || probe.height != 2) {
            fprintf(stderr,
                    "%s: geometry mismatch (%dx%d)\n",
                    hdr_orientation_cases[case_index].label,
                    probe.width,
                    probe.height);
            return 1;
        }
        for (sample_index = 0u;
             sample_index <
             sizeof(expected_pixels) / sizeof(expected_pixels[0]);
             ++sample_index) {
            if (!float_approx_equal(probe.pixels[sample_index],
                                    expected_pixels[sample_index],
                                    tolerance)) {
                fprintf(stderr,
                        "%s: sample %zu mismatch (actual=%f expected=%f)\n",
                        hdr_orientation_cases[case_index].label,
                        sample_index,
                        probe.pixels[sample_index],
                        expected_pixels[sample_index]);
                return 1;
            }
        }
    }

    return 0;
}

typedef enum hdr_numeric_dual_expect_mode {
    HDR_NUMERIC_DUAL_EXPECT_SRGB_FROM_BASELINE = 0,
    HDR_NUMERIC_DUAL_EXPECT_FALLBACK_FROM_BASELINE,
    HDR_NUMERIC_DUAL_EXPECT_SAME_AS_BASELINE
} hdr_numeric_dual_expect_mode_t;

typedef struct hdr_numeric_dual_case_spec {
    char const *sample_path;
    char const *baseline_label;
    int baseline_cms_engine;
    int baseline_pixelformat;
    int baseline_colorspace;
    char const *variant_label;
    int variant_cms_engine;
    int variant_pixelformat;
    int variant_colorspace;
    float baseline_expected[3];
    float tolerance;
    int require_target_pixelformat;
    int target_pixelformat;
    hdr_numeric_dual_expect_mode_t expect_mode;
    int require_variant_channel1_delta;
    float min_variant_channel1_delta;
    int require_source_profile_observed;
    float expected_diff_threshold;
    float observed_diff_min;
} hdr_numeric_dual_case_spec_t;

typedef enum hdr_numeric_dual_case_id {
    HDR_NUMERIC_DUAL_CASE_GAMMA = 0,
    HDR_NUMERIC_DUAL_CASE_FALLBACK_PROFILE,
    HDR_NUMERIC_DUAL_CASE_HEADER_PRIORITY
} hdr_numeric_dual_case_id_t;

static int compute_hdr_srgb_fallback_expected(float *rgb_linear);

static hdr_numeric_dual_case_spec_t const hdr_numeric_dual_cases[] = {
    {
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        "builtin loader hdr numeric cms=off",
        SIXEL_CMS_ENGINE_NONE,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        "builtin loader hdr numeric cms=on",
        SIXEL_CMS_ENGINE_AUTO,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA,
        { 0.5f, 0.25f, 0.125f },
        0.0005f,
        1,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        HDR_NUMERIC_DUAL_EXPECT_SRGB_FROM_BASELINE,
        1,
        0.1f,
        0,
        0.0f,
        0.0f
    },
    {
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        "builtin loader hdr fallback numeric cms=off",
        SIXEL_CMS_ENGINE_NONE,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        "builtin loader hdr fallback numeric cms=on",
        SIXEL_CMS_ENGINE_BUILTIN,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.5f, 0.25f, 0.125f },
        0.0007f,
        1,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        HDR_NUMERIC_DUAL_EXPECT_FALLBACK_FROM_BASELINE,
        0,
        0.0f,
        1,
        0.05f,
        0.05f
    },
    {
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_linear.hdr",
        "builtin loader hdr header-priority numeric cms=off",
        SIXEL_CMS_ENGINE_NONE,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        "builtin loader hdr header-priority numeric cms=on",
        SIXEL_CMS_ENGINE_BUILTIN,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_COLORSPACE_LINEAR,
        { 0.5f, 0.25f, 0.125f },
        0.0007f,
        1,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        HDR_NUMERIC_DUAL_EXPECT_SAME_AS_BASELINE,
        0,
        0.0f,
        0,
        0.0f,
        0.0f
    }
};

static int
run_builtin_loader_hdr_dual_numeric_case(
    hdr_numeric_dual_case_spec_t const *spec)
{
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t variant_probe;
    float expected_variant[3];
    int index;
    int result;

    baseline_probe = (hdr_numeric_probe_context_t){ 0 };
    variant_probe = (hdr_numeric_probe_context_t){ 0 };
    memset(expected_variant, 0, sizeof(expected_variant));
    index = 0;
    result = 1;
    if (spec == NULL) {
        return 1;
    }
    if (spec->require_target_pixelformat != 0 &&
        loader_cms_target_pixelformat() != spec->target_pixelformat) {
        fprintf(stderr,
                "%s: unexpected cms target pixelformat (%d expected=%d)\n",
                spec->baseline_label,
                loader_cms_target_pixelformat(),
                spec->target_pixelformat);
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        spec->baseline_label,
        spec->sample_path,
        spec->baseline_cms_engine,
        &baseline_probe);
    if (result != 0) {
        return result;
    }
    if (baseline_probe.pixelformat != spec->baseline_pixelformat ||
        baseline_probe.colorspace != spec->baseline_colorspace) {
        fprintf(stderr,
                "%s: frame contract mismatch (pf=%d expected=%d cs=%d "
                "expected=%d)\n",
                spec->baseline_label,
                baseline_probe.pixelformat,
                spec->baseline_pixelformat,
                baseline_probe.colorspace,
                spec->baseline_colorspace);
        return 1;
    }
    for (index = 0; index < 3; ++index) {
        if (!float_approx_equal(baseline_probe.first_pixel[index],
                                spec->baseline_expected[index],
                                spec->tolerance)) {
            fprintf(stderr,
                    "%s: channel %d mismatch (actual=%f expected=%f)\n",
                    spec->baseline_label,
                    index,
                    baseline_probe.first_pixel[index],
                    spec->baseline_expected[index]);
            return 1;
        }
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        spec->variant_label,
        spec->sample_path,
        spec->variant_cms_engine,
        &variant_probe);
    if (result != 0) {
        return result;
    }
    if (variant_probe.pixelformat != spec->variant_pixelformat ||
        variant_probe.colorspace != spec->variant_colorspace) {
        fprintf(stderr,
                "%s: frame contract mismatch (pf=%d expected=%d cs=%d "
                "expected=%d)\n",
                spec->variant_label,
                variant_probe.pixelformat,
                spec->variant_pixelformat,
                variant_probe.colorspace,
                spec->variant_colorspace);
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        expected_variant[index] = baseline_probe.first_pixel[index];
    }
    if (spec->expect_mode == HDR_NUMERIC_DUAL_EXPECT_SRGB_FROM_BASELINE) {
        for (index = 0; index < 3; ++index) {
            expected_variant[index] = srgb_from_linear(expected_variant[index]);
        }
    } else if (spec->expect_mode ==
               HDR_NUMERIC_DUAL_EXPECT_FALLBACK_FROM_BASELINE) {
        if (!compute_hdr_srgb_fallback_expected(expected_variant)) {
            fprintf(stderr,
                    "%s: failed to compute expected fallback conversion\n",
                    spec->variant_label);
            return 1;
        }
    }

    for (index = 0; index < 3; ++index) {
        if (!float_approx_equal(variant_probe.first_pixel[index],
                                expected_variant[index],
                                spec->tolerance)) {
            fprintf(stderr,
                    "%s: channel %d mismatch (actual=%f expected=%f)\n",
                    spec->variant_label,
                    index,
                    variant_probe.first_pixel[index],
                    expected_variant[index]);
            return 1;
        }
    }

    if (spec->require_variant_channel1_delta != 0 &&
        fabsf(variant_probe.first_pixel[1] -
              baseline_probe.first_pixel[1]) <
            spec->min_variant_channel1_delta) {
        fprintf(stderr,
                "%s: expected channel1 delta was not observed\n",
                spec->variant_label);
        return 1;
    }

    if (spec->require_source_profile_observed != 0 &&
        fabsf(expected_variant[0] - baseline_probe.first_pixel[0]) >
            spec->expected_diff_threshold &&
        fabsf(variant_probe.first_pixel[0] -
              baseline_probe.first_pixel[0]) <
            spec->observed_diff_min) {
        fprintf(stderr,
                "%s: expected source profile conversion was not observed\n",
                spec->variant_label);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_hdr_gamma_numeric_test(void)
{
    return run_builtin_loader_hdr_dual_numeric_case(
        &hdr_numeric_dual_cases[HDR_NUMERIC_DUAL_CASE_GAMMA]);
}

static int
compute_hdr_srgb_fallback_expected(float *rgb_linear)
{
    sixel_cms_profile_t *src_profile;
    int converted;

    src_profile = NULL;
    converted = 0;
    if (rgb_linear == NULL) {
        return 0;
    }

    src_profile = sixel_cms_create_srgb_profile();
    if (src_profile == NULL) {
        return 0;
    }
    converted = sixel_cms_convert_profile_to_linearrgb(
        (unsigned char *)rgb_linear,
        1,
        1,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        src_profile);
    sixel_cms_close_profile(src_profile);

    return converted;
}

static int
run_builtin_loader_hdr_fallback_profile_numeric_test(void)
{
    return run_builtin_loader_hdr_dual_numeric_case(
        &hdr_numeric_dual_cases[HDR_NUMERIC_DUAL_CASE_FALLBACK_PROFILE]);
}

static int
run_builtin_loader_hdr_header_priority_numeric_test(void)
{
    return run_builtin_loader_hdr_dual_numeric_case(
        &hdr_numeric_dual_cases[HDR_NUMERIC_DUAL_CASE_HEADER_PRIORITY]);
}

static int
run_builtin_loader_hdr_exposure_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[HDR_NUMERIC_STATIC_CASE_EXPOSURE]);
}

static int
run_builtin_loader_hdr_tonemap_reinhard_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[HDR_NUMERIC_STATIC_CASE_TONEMAP_REINHARD]);
}

static int
run_builtin_loader_hdr_header_exposure_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[HDR_NUMERIC_STATIC_CASE_HEADER_EXPOSURE]);
}

static int
run_builtin_loader_hdr_header_exposure_multi_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[
            HDR_NUMERIC_STATIC_CASE_HEADER_EXPOSURE_MULTI]);
}

static int
run_builtin_loader_hdr_header_exposure_disabled_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[
            HDR_NUMERIC_STATIC_CASE_HEADER_EXPOSURE_DISABLED]);
}

static int
run_builtin_loader_hdr_xyze_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[HDR_NUMERIC_STATIC_CASE_XYZE_FORMAT]);
}

static int
run_builtin_loader_hdr_colorcorr_single_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[HDR_NUMERIC_STATIC_CASE_COLORCORR_SINGLE]);
}

static int
run_builtin_loader_hdr_colorcorr_multi_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[HDR_NUMERIC_STATIC_CASE_COLORCORR_MULTI]);
}

static int
run_builtin_loader_hdr_partial_header_numeric_test(
    char const *label,
    char const *sample_path,
    hdr_test_gamma_mode_t gamma_mode,
    hdr_test_primaries_mode_t primaries_mode)
{
    hdr_numeric_probe_context_t probe;
    float expected[3];
    float tolerance;
    int target_pixelformat;
    int target_colorspace;
    int channel;
    int result;

    tolerance = 0.0012f;
    target_pixelformat = loader_cms_target_pixelformat();
    target_colorspace = expected_colorspace_for_pixelformat(target_pixelformat);
    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(label,
                                                       sample_path,
                                                       SIXEL_CMS_ENGINE_AUTO,
                                                       &probe);
    if (result != 0) {
        return result;
    }
    if (probe.pixelformat != target_pixelformat ||
        probe.colorspace != target_colorspace) {
        fprintf(stderr,
                "%s: frame contract mismatch (pf=%d expected=%d cs=%d "
                "expected=%d)\n",
                label,
                probe.pixelformat,
                target_pixelformat,
                probe.colorspace,
                target_colorspace);
        return 1;
    }

    if (hdr_test_expected_first_pixel(expected,
                                      gamma_mode,
                                      primaries_mode,
                                      0.0,
                                      HDR_TEST_TONEMAP_NONE,
                                      SIXEL_CMS_ENGINE_AUTO,
                                      HDR_TEST_FALLBACK_LINEAR_SRGB,
                                      target_pixelformat) != 0) {
        fprintf(stderr, "%s: failed to compute expected value\n", label);
        return 1;
    }
    for (channel = 0; channel < 3; ++channel) {
        if (!float_approx_equal(probe.first_pixel[channel],
                                expected[channel],
                                tolerance)) {
            fprintf(stderr,
                    "%s: channel %d mismatch (actual=%f expected=%f)\n",
                    label,
                    channel,
                    probe.first_pixel[channel],
                    expected[channel]);
            return 1;
        }
    }

    return 0;
}

static int
SIXEL_T0014_FN(SIXEL_T0014_PREF_HDR_GINV_PVALID_NUM, _test)(void)
{
    return run_builtin_loader_hdr_partial_header_numeric_test(
        "builtin loader hdr gamma invalid + primaries valid numeric",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_gamma_invalid_bt2020.hdr",
        HDR_TEST_GAMMA_NONE,
        HDR_TEST_PRIMARIES_BT2020);
}

static int
SIXEL_T0014_FN(SIXEL_T0014_PREF_HDR_PINV_GVALID_NUM, _test)(void)
{
    return run_builtin_loader_hdr_partial_header_numeric_test(
        "builtin loader hdr primaries invalid + gamma valid numeric",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_gamma22_primaries_invalid.hdr",
        HDR_TEST_GAMMA_22,
        HDR_TEST_PRIMARIES_NONE);
}

static int
hdr_test_compare_probe(char const *label,
                       hdr_numeric_probe_context_t const *left,
                       hdr_numeric_probe_context_t const *right,
                       float tolerance);

typedef struct hdr_numeric_compare_case_spec {
    char const *setup_fallback_profile;
    char const *setup_exposure_ev;
    char const *setup_tonemap;
    char const *setup_use_header_exposure;
    char const *baseline_label;
    char const *baseline_path;
    int baseline_cms_engine;
    char const *variant_label;
    char const *variant_path;
    int variant_cms_engine;
    char const *variant_env_name;
    char const *variant_env_value;
    char const *compare_label;
    float tolerance;
} hdr_numeric_compare_case_spec_t;

typedef enum hdr_numeric_compare_case_id {
    HDR_NUMERIC_COMPARE_INVALID_HEADER_EXPOSURE = 0,
    HDR_NUMERIC_COMPARE_MIXED_HEADER_EXPOSURE_INVALID,
    HDR_NUMERIC_COMPARE_INVALID_USE_HEADER_EXPOSURE_ENV,
    HDR_NUMERIC_COMPARE_DUPLICATE_HEADER_METADATA,
    HDR_NUMERIC_COMPARE_PIXASPECT_VIEW_METADATA,
    HDR_NUMERIC_COMPARE_INVALID_FALLBACK,
    HDR_NUMERIC_COMPARE_INVALID_TONEMAP,
    HDR_NUMERIC_COMPARE_INVALID_EXPOSURE
} hdr_numeric_compare_case_id_t;

static hdr_numeric_compare_case_spec_t const hdr_numeric_compare_cases[] = {
    {
        "linear-srgb",
        "0",
        "none",
        "1",
        "builtin hdr invalid header exposure baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "builtin hdr invalid header exposure",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_exposure_invalid_zero.hdr",
        SIXEL_CMS_ENGINE_NONE,
        NULL,
        NULL,
        "builtin hdr invalid header exposure",
        0.0007f
    },
    {
        "linear-srgb",
        "0",
        "none",
        "1",
        "builtin hdr mixed header exposure invalid baseline",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "builtin hdr mixed header exposure invalid",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_exposure2_invalid_overflow.hdr",
        SIXEL_CMS_ENGINE_NONE,
        NULL,
        NULL,
        "builtin hdr mixed header exposure invalid",
        0.0007f
    },
    {
        "linear-srgb",
        "0",
        "none",
        "1",
        "builtin hdr invalid use-header-exposure env baseline",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "builtin hdr invalid use-header-exposure env",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE",
        "invalid-value",
        "builtin hdr invalid use-header-exposure env",
        0.0007f
    },
    {
        "linear-srgb",
        "0",
        "none",
        "1",
        "builtin hdr duplicate metadata baseline",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_gamma22_bt2020.hdr",
        SIXEL_CMS_ENGINE_AUTO,
        "builtin hdr duplicate metadata",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_duplicate_gamma_primaries_last_wins.hdr",
        SIXEL_CMS_ENGINE_AUTO,
        NULL,
        NULL,
        "builtin hdr duplicate metadata",
        0.0012f
    },
    {
        "linear-srgb",
        "0",
        "none",
        "1",
        "builtin hdr pixaspect/view baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "builtin hdr pixaspect/view metadata",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_pixaspect_view.hdr",
        SIXEL_CMS_ENGINE_NONE,
        NULL,
        NULL,
        "builtin hdr pixaspect/view metadata",
        0.0007f
    },
    {
        "linear-srgb",
        "0",
        "none",
        "1",
        "builtin hdr invalid fallback baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_AUTO,
        "builtin hdr invalid fallback",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_AUTO,
        "SIXEL_LOADER_HDR_FALLBACK_PROFILE",
        "invalid-profile",
        "builtin hdr invalid fallback",
        0.0007f
    },
    {
        "linear-srgb",
        "0",
        "none",
        "1",
        "builtin hdr invalid tonemap baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "builtin hdr invalid tonemap",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "SIXEL_LOADER_HDR_TONEMAP",
        "invalid-tonemap",
        "builtin hdr invalid tonemap",
        0.0007f
    },
    {
        "linear-srgb",
        "0",
        "none",
        "1",
        "builtin hdr invalid exposure baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "builtin hdr invalid exposure",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        "SIXEL_LOADER_HDR_EXPOSURE_EV",
        "not-a-number",
        "builtin hdr invalid exposure",
        0.0007f
    }
};

static int
run_builtin_loader_hdr_compare_case(
    hdr_numeric_compare_case_spec_t const *spec)
{
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t variant_probe;
    int result;

    baseline_probe = (hdr_numeric_probe_context_t){ 0 };
    variant_probe = (hdr_numeric_probe_context_t){ 0 };
    result = 1;
    if (spec == NULL) {
        return 1;
    }

    if (hdr_test_configure_loader_env(spec->setup_fallback_profile,
                                      spec->setup_exposure_ev,
                                      spec->setup_tonemap,
                                      spec->setup_use_header_exposure) != 0) {
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        spec->baseline_label,
        spec->baseline_path,
        spec->baseline_cms_engine,
        &baseline_probe);
    if (result != 0) {
        return result;
    }

    if (spec->variant_env_name != NULL) {
        if (spec->variant_env_value == NULL ||
            loader_test_setenv(spec->variant_env_name,
                               spec->variant_env_value) != 0) {
            return 1;
        }
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        spec->variant_label,
        spec->variant_path,
        spec->variant_cms_engine,
        &variant_probe);
    if (result != 0) {
        return result;
    }

    return hdr_test_compare_probe(spec->compare_label,
                                  &variant_probe,
                                  &baseline_probe,
                                  spec->tolerance);
}

static int
run_builtin_loader_hdr_invalid_header_exposure_numeric_test(void)
{
    return run_builtin_loader_hdr_compare_case(
        &hdr_numeric_compare_cases[
            HDR_NUMERIC_COMPARE_INVALID_HEADER_EXPOSURE]);
}

static int
SIXEL_T0014_FN(SIXEL_T0014_PREF_HDR_MIX_HDR_EXP_INV_NUM, _test)(void)
{
    return run_builtin_loader_hdr_compare_case(
        &hdr_numeric_compare_cases[
            HDR_NUMERIC_COMPARE_MIXED_HEADER_EXPOSURE_INVALID]);
}

static int
run_builtin_loader_hdr_invalid_use_hdr_exposure_env_test(void)
{
    return run_builtin_loader_hdr_compare_case(
        &hdr_numeric_compare_cases[
            HDR_NUMERIC_COMPARE_INVALID_USE_HEADER_EXPOSURE_ENV]);
}

static int
run_builtin_loader_hdr_duplicate_header_metadata_numeric_test(void)
{
    return run_builtin_loader_hdr_compare_case(
        &hdr_numeric_compare_cases[
            HDR_NUMERIC_COMPARE_DUPLICATE_HEADER_METADATA]);
}

static int
run_builtin_loader_hdr_pixaspect_view_metadata_numeric_test(void)
{
    return run_builtin_loader_hdr_compare_case(
        &hdr_numeric_compare_cases[
            HDR_NUMERIC_COMPARE_PIXASPECT_VIEW_METADATA]);
}

static char const *
hdr_test_fixture_for_axes(hdr_test_gamma_mode_t gamma_mode,
                          hdr_test_primaries_mode_t primaries_mode)
{
    if (gamma_mode == HDR_TEST_GAMMA_NONE &&
        primaries_mode == HDR_TEST_PRIMARIES_NONE) {
        return "/tests/data/inputs/formats/stbi_midtones.hdr";
    }
    if (gamma_mode == HDR_TEST_GAMMA_22 &&
        primaries_mode == HDR_TEST_PRIMARIES_NONE) {
        return "/tests/data/inputs/formats/stbi_midtones_hdrmeta_gamma22.hdr";
    }
    if (gamma_mode == HDR_TEST_GAMMA_NONE &&
        primaries_mode == HDR_TEST_PRIMARIES_BT2020) {
        return "/tests/data/inputs/formats/stbi_midtones_hdrmeta_bt2020.hdr";
    }
    return "/tests/data/inputs/formats/"
           "stbi_midtones_hdrmeta_gamma22_bt2020.hdr";
}

static int
hdr_test_compare_probe(char const *label,
                       hdr_numeric_probe_context_t const *left,
                       hdr_numeric_probe_context_t const *right,
                       float tolerance)
{
    int index;

    if (label == NULL || left == NULL || right == NULL) {
        return 1;
    }
    if (left->pixelformat != right->pixelformat ||
        left->colorspace != right->colorspace) {
        fprintf(stderr,
                "%s: frame contract mismatch "
                "(actual pf=%d cs=%d, expected pf=%d cs=%d)\n",
                label,
                left->pixelformat,
                left->colorspace,
                right->pixelformat,
                right->colorspace);
        return 1;
    }
    for (index = 0; index < 3; ++index) {
        if (!float_approx_equal(left->first_pixel[index],
                                right->first_pixel[index],
                                tolerance)) {
            fprintf(stderr,
                    "%s: channel %d mismatch (actual=%f expected=%f)\n",
                    label,
                    index,
                    left->first_pixel[index],
                    right->first_pixel[index]);
            return 1;
        }
    }
    return 0;
}

static int
hdr_test_parse_choice(char const *text,
                      int default_value,
                      char const * const *keys,
                      int const *values,
                      size_t count,
                      int *out_value)
{
    size_t index;

    index = 0u;
    if (keys == NULL || values == NULL || out_value == NULL) {
        return 1;
    }
    if (text == NULL || text[0] == '\0') {
        *out_value = default_value;
        return 0;
    }

    for (index = 0u; index < count; ++index) {
        if (strcmp(text, keys[index]) == 0) {
            *out_value = values[index];
            return 0;
        }
    }
    return 1;
}

static int
hdr_test_parse_gamma_mode(char const *text,
                          hdr_test_gamma_mode_t *out_mode)
{
    static char const * const keys[] = {
        "none",
        "2.2",
        "2_2"
    };
    static int const values[] = {
        HDR_TEST_GAMMA_NONE,
        HDR_TEST_GAMMA_22,
        HDR_TEST_GAMMA_22
    };
    int parsed;

    parsed = HDR_TEST_GAMMA_NONE;
    if (out_mode == NULL) {
        return 1;
    }
    if (hdr_test_parse_choice(text,
                              HDR_TEST_GAMMA_NONE,
                              keys,
                              values,
                              sizeof(keys) / sizeof(keys[0]),
                              &parsed) != 0) {
        return 1;
    }
    *out_mode = (hdr_test_gamma_mode_t)parsed;
    return 0;
}

static int
hdr_test_parse_primaries_mode(char const *text,
                              hdr_test_primaries_mode_t *out_mode)
{
    static char const * const keys[] = {
        "none",
        "bt2020"
    };
    static int const values[] = {
        HDR_TEST_PRIMARIES_NONE,
        HDR_TEST_PRIMARIES_BT2020
    };
    int parsed;

    parsed = HDR_TEST_PRIMARIES_NONE;
    if (out_mode == NULL) {
        return 1;
    }
    if (hdr_test_parse_choice(text,
                              HDR_TEST_PRIMARIES_NONE,
                              keys,
                              values,
                              sizeof(keys) / sizeof(keys[0]),
                              &parsed) != 0) {
        return 1;
    }
    *out_mode = (hdr_test_primaries_mode_t)parsed;
    return 0;
}

static int
hdr_test_parse_tonemap_mode(char const *text,
                            hdr_test_tonemap_mode_t *out_mode)
{
    static char const * const keys[] = {
        "none",
        "reinhard"
    };
    static int const values[] = {
        HDR_TEST_TONEMAP_NONE,
        HDR_TEST_TONEMAP_REINHARD
    };
    int parsed;

    parsed = HDR_TEST_TONEMAP_NONE;
    if (out_mode == NULL) {
        return 1;
    }
    if (hdr_test_parse_choice(text,
                              HDR_TEST_TONEMAP_NONE,
                              keys,
                              values,
                              sizeof(keys) / sizeof(keys[0]),
                              &parsed) != 0) {
        return 1;
    }
    *out_mode = (hdr_test_tonemap_mode_t)parsed;
    return 0;
}

static int
hdr_test_parse_fallback_mode(char const *text,
                             hdr_test_fallback_mode_t *out_mode)
{
    static char const * const keys[] = {
        "linear-srgb",
        "srgb"
    };
    static int const values[] = {
        HDR_TEST_FALLBACK_LINEAR_SRGB,
        HDR_TEST_FALLBACK_SRGB
    };
    int parsed;

    parsed = HDR_TEST_FALLBACK_LINEAR_SRGB;
    if (out_mode == NULL) {
        return 1;
    }
    if (hdr_test_parse_choice(text,
                              HDR_TEST_FALLBACK_LINEAR_SRGB,
                              keys,
                              values,
                              sizeof(keys) / sizeof(keys[0]),
                              &parsed) != 0) {
        return 1;
    }
    *out_mode = (hdr_test_fallback_mode_t)parsed;
    return 0;
}

static int
hdr_test_parse_cms_engine(char const *text, int *out_cms_engine)
{
    static char const * const keys[] = {
        "none",
        "auto"
    };
    static int const values[] = {
        SIXEL_CMS_ENGINE_NONE,
        SIXEL_CMS_ENGINE_AUTO
    };
    int parsed;

    parsed = SIXEL_CMS_ENGINE_NONE;
    if (out_cms_engine == NULL) {
        return 1;
    }
    if (hdr_test_parse_choice(text,
                              SIXEL_CMS_ENGINE_NONE,
                              keys,
                              values,
                              sizeof(keys) / sizeof(keys[0]),
                              &parsed) != 0) {
        return 1;
    }
    *out_cms_engine = parsed;
    return 0;
}

static int
hdr_test_parse_exposure_ev(char const *text, double *out_exposure)
{
    char *endptr;
    double value;

    if (out_exposure == NULL) {
        return 1;
    }
    if (text == NULL || text[0] == '\0') {
        *out_exposure = 0.0;
        return 0;
    }

    endptr = NULL;
    value = strtod(text, &endptr);
    if (endptr == text || endptr == NULL || endptr[0] != '\0' ||
        !isfinite(value)) {
        return 1;
    }

    *out_exposure = value;
    return 0;
}

static int
run_builtin_loader_hdr_single_case_numeric_test(void)
{
    char const *gamma_text;
    char const *primaries_text;
    char const *exposure_text;
    char const *tonemap_text;
    char const *cms_text;
    char const *fallback_text;
    hdr_test_gamma_mode_t gamma_mode;
    hdr_test_primaries_mode_t primaries_mode;
    hdr_test_tonemap_mode_t tonemap_mode;
    hdr_test_fallback_mode_t fallback_mode;
    int cms_engine;
    double exposure_ev;
    char const *sample_path;
    hdr_numeric_probe_context_t probe;
    float expected[3];
    float tolerance;
    int target_pixelformat;
    int target_colorspace;
    int expected_pixelformat;
    int expected_colorspace;
    int channel;
    int result;
    char label[256];

    gamma_mode = HDR_TEST_GAMMA_NONE;
    primaries_mode = HDR_TEST_PRIMARIES_NONE;
    tonemap_mode = HDR_TEST_TONEMAP_NONE;
    fallback_mode = HDR_TEST_FALLBACK_LINEAR_SRGB;
    cms_engine = SIXEL_CMS_ENGINE_NONE;
    exposure_ev = 0.0;
    tolerance = 0.0012f;
    target_pixelformat = loader_cms_target_pixelformat();
    target_colorspace = expected_colorspace_for_pixelformat(target_pixelformat);

    gamma_text = loader_test_getenv("SIXEL_TEST_HDR_CASE_GAMMA");
    primaries_text = loader_test_getenv("SIXEL_TEST_HDR_CASE_PRIMARIES");
    exposure_text = loader_test_getenv("SIXEL_TEST_HDR_CASE_EXPOSURE_EV");
    tonemap_text = loader_test_getenv("SIXEL_TEST_HDR_CASE_TONEMAP");
    cms_text = loader_test_getenv("SIXEL_TEST_HDR_CASE_CMS_ENGINE");
    fallback_text = loader_test_getenv("SIXEL_TEST_HDR_CASE_FALLBACK_PROFILE");

    if (hdr_test_parse_gamma_mode(gamma_text, &gamma_mode) != 0 ||
        hdr_test_parse_primaries_mode(primaries_text, &primaries_mode) != 0 ||
        hdr_test_parse_exposure_ev(exposure_text, &exposure_ev) != 0 ||
        hdr_test_parse_tonemap_mode(tonemap_text, &tonemap_mode) != 0 ||
        hdr_test_parse_cms_engine(cms_text, &cms_engine) != 0 ||
        hdr_test_parse_fallback_mode(fallback_text, &fallback_mode) != 0) {
        fprintf(stderr,
                "builtin loader hdr single-case numeric: "
                "invalid case env values\n");
        return 1;
    }

    if (fallback_text == NULL || fallback_text[0] == '\0') {
        fallback_text = "linear-srgb";
    }
    if (tonemap_text == NULL || tonemap_text[0] == '\0') {
        tonemap_text = "none";
    }
    if (exposure_text == NULL || exposure_text[0] == '\0') {
        exposure_text = "0";
    }
    if (cms_text == NULL || cms_text[0] == '\0') {
        cms_text = "none";
    }
    if (hdr_test_configure_loader_env(fallback_text,
                                      exposure_text,
                                      tonemap_text,
                                      "1") != 0) {
        fprintf(stderr,
                "builtin loader hdr single-case numeric: "
                "failed to set loader env\n");
        return 1;
    }

    sample_path = hdr_test_fixture_for_axes(gamma_mode, primaries_mode);
    snprintf(label,
             sizeof(label),
             "builtin hdr single-case g=%s p=%s ev=%s tm=%s cms=%s fb=%s",
             gamma_text != NULL ? gamma_text : "none",
             primaries_text != NULL ? primaries_text : "none",
             exposure_text,
             tonemap_text,
             cms_text,
             fallback_text);

    result = run_builtin_loader_hdr_numeric_probe_case(label,
                                                       sample_path,
                                                       cms_engine,
                                                       &probe);
    if (result != 0) {
        return result;
    }

    if (cms_engine == SIXEL_CMS_ENGINE_NONE) {
        expected_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        expected_colorspace = SIXEL_COLORSPACE_LINEAR;
    } else {
        expected_pixelformat = target_pixelformat;
        expected_colorspace = target_colorspace;
    }
    if (probe.pixelformat != expected_pixelformat ||
        probe.colorspace != expected_colorspace) {
        fprintf(stderr,
                "%s: frame contract mismatch "
                "(pf=%d expected=%d cs=%d expected=%d)\n",
                label,
                probe.pixelformat,
                expected_pixelformat,
                probe.colorspace,
                expected_colorspace);
        return 1;
    }

    if (hdr_test_expected_first_pixel(expected,
                                      gamma_mode,
                                      primaries_mode,
                                      exposure_ev,
                                      tonemap_mode,
                                      cms_engine,
                                      fallback_mode,
                                      target_pixelformat) != 0) {
        fprintf(stderr, "%s: failed to compute expected value\n", label);
        return 1;
    }

    for (channel = 0; channel < 3; ++channel) {
        if (!float_approx_equal(probe.first_pixel[channel],
                                expected[channel],
                                tolerance)) {
            fprintf(stderr,
                    "%s: channel %d mismatch (actual=%f expected=%f)\n",
                    label,
                    channel,
                    probe.first_pixel[channel],
                    expected[channel]);
            return 1;
        }
    }

    return 0;
}

static int
run_builtin_loader_hdr_invalid_fallback_numeric_test(void)
{
    return run_builtin_loader_hdr_compare_case(
        &hdr_numeric_compare_cases[HDR_NUMERIC_COMPARE_INVALID_FALLBACK]);
}

static int
run_builtin_loader_hdr_invalid_tonemap_numeric_test(void)
{
    return run_builtin_loader_hdr_compare_case(
        &hdr_numeric_compare_cases[HDR_NUMERIC_COMPARE_INVALID_TONEMAP]);
}

static int
run_builtin_loader_hdr_invalid_exposure_numeric_test(void)
{
    return run_builtin_loader_hdr_compare_case(
        &hdr_numeric_compare_cases[HDR_NUMERIC_COMPARE_INVALID_EXPOSURE]);
}
