static SIXELSTATUS
new_builtin_component_for_pixelformat_test(sixel_allocator_t *allocator,
                                           void **ppcomponent)
{
    return create_loader_component_by_name("builtin", allocator, ppcomponent);
}

static int
expected_colorspace_for_pixelformat(int pixelformat)
{
    switch (pixelformat) {
    case SIXEL_PIXELFORMAT_LINEARRGBFLOAT32:
        return SIXEL_COLORSPACE_LINEAR;
    case SIXEL_PIXELFORMAT_CIELABFLOAT32:
        return SIXEL_COLORSPACE_CIELAB;
    case SIXEL_PIXELFORMAT_OKLABFLOAT32:
        return SIXEL_COLORSPACE_OKLAB;
    case SIXEL_PIXELFORMAT_DIN99DFLOAT32:
        return SIXEL_COLORSPACE_DIN99D;
    case SIXEL_PIXELFORMAT_RGBFLOAT32:
    case SIXEL_PIXELFORMAT_RGB888:
    default:
        return SIXEL_COLORSPACE_GAMMA;
    }
}

static int
parse_expected_hdr_cms_pixelformat(char const *text, int *out_pixelformat)
{
    if (text == NULL || out_pixelformat == NULL) {
        return -1;
    }
    if (strcmp(text, "LINEARRGBFLOAT32") == 0) {
        *out_pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
        return 0;
    }
    if (strcmp(text, "RGBFLOAT32") == 0) {
        *out_pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        return 0;
    }
    if (strcmp(text, "CIELABFLOAT32") == 0) {
        *out_pixelformat = SIXEL_PIXELFORMAT_CIELABFLOAT32;
        return 0;
    }
    if (strcmp(text, "OKLABFLOAT32") == 0) {
        *out_pixelformat = SIXEL_PIXELFORMAT_OKLABFLOAT32;
        return 0;
    }
    if (strcmp(text, "DIN99DFLOAT32") == 0) {
        *out_pixelformat = SIXEL_PIXELFORMAT_DIN99DFLOAT32;
        return 0;
    }
    if (strcmp(text, "RGB888") == 0) {
        *out_pixelformat = SIXEL_PIXELFORMAT_RGB888;
        return 0;
    }
    return -1;
}

typedef struct hdr_numeric_probe_context {
    int callback_count;
    int pixelformat;
    int colorspace;
    int width;
    int height;
    float first_pixel[3];
} hdr_numeric_probe_context_t;

typedef struct gif_loop_probe_context {
    int callback_count;
    int max_callbacks;
    int required_loop_no;
    int highest_loop_no;
    int highest_frame_no;
    int saw_multiframe;
    int reached_required_loop;
} gif_loop_probe_context_t;

typedef struct gif_sequence_probe_context {
    int callback_count;
    int expected_count;
    int mismatch_index;
    int mismatch_loop_no;
    int mismatch_frame_no;
    int saw_multiframe;
    int const *expected_sequence;
} gif_sequence_probe_context_t;

typedef enum hdr_test_gamma_mode {
    HDR_TEST_GAMMA_NONE = 0,
    HDR_TEST_GAMMA_22
} hdr_test_gamma_mode_t;

typedef enum hdr_test_primaries_mode {
    HDR_TEST_PRIMARIES_NONE = 0,
    HDR_TEST_PRIMARIES_BT2020
} hdr_test_primaries_mode_t;

typedef enum hdr_test_tonemap_mode {
    HDR_TEST_TONEMAP_NONE = 0,
    HDR_TEST_TONEMAP_REINHARD
} hdr_test_tonemap_mode_t;

typedef enum hdr_test_fallback_mode {
    HDR_TEST_FALLBACK_LINEAR_SRGB = 0,
    HDR_TEST_FALLBACK_SRGB
} hdr_test_fallback_mode_t;

#define HDR_TEST_SRGB_WHITE_X 0.3127
#define HDR_TEST_SRGB_WHITE_Y 0.3290
#define HDR_TEST_SRGB_RED_X   0.6400
#define HDR_TEST_SRGB_RED_Y   0.3300
#define HDR_TEST_SRGB_GREEN_X 0.3000
#define HDR_TEST_SRGB_GREEN_Y 0.6000
#define HDR_TEST_SRGB_BLUE_X  0.1500
#define HDR_TEST_SRGB_BLUE_Y  0.0600

#define HDR_TEST_BT2020_RED_X   0.7080
#define HDR_TEST_BT2020_RED_Y   0.2919
#define HDR_TEST_BT2020_GREEN_X 0.1700
#define HDR_TEST_BT2020_GREEN_Y 0.7970
#define HDR_TEST_BT2020_BLUE_X  0.1310
#define HDR_TEST_BT2020_BLUE_Y  0.0460

static SIXELSTATUS
capture_hdr_numeric_probe(sixel_frame_t *frame, void *data)
{
    hdr_numeric_probe_context_t *context;
    float const *pixels;
    sixel_frame_pixels_view_t view;
    SIXELSTATUS status;

    context = (hdr_numeric_probe_context_t *)data;
    pixels = NULL;
    memset(&view, 0, sizeof(view));
    status = SIXEL_FALSE;
    if (context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    context->callback_count += 1;
    context->pixelformat = sixel_frame_get_pixelformat(frame);
    context->colorspace = sixel_frame_get_colorspace(frame);
    context->width = sixel_frame_get_width(frame);
    context->height = sixel_frame_get_height(frame);
    context->first_pixel[0] = 0.0f;
    context->first_pixel[1] = 0.0f;
    context->first_pixel[2] = 0.0f;

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
    context->first_pixel[0] = pixels[0];
    context->first_pixel[1] = pixels[1];
    context->first_pixel[2] = pixels[2];

    return SIXEL_OK;
}

static SIXELSTATUS
capture_gif_loop_probe_until_target(sixel_frame_t *frame, void *data)
{
    gif_loop_probe_context_t *context;
    int loop_no;
    int frame_no;

    context = (gif_loop_probe_context_t *)data;
    loop_no = 0;
    frame_no = 0;
    if (context == NULL || frame == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Force-loop cases are intentionally unbounded, so this callback ends the
     * decode by returning SIXEL_INTERRUPTED once the required loop number is
     * observed. The max-callback guard prevents runaway decode on regressions.
     */
    context->callback_count += 1;
    loop_no = sixel_frame_get_loop_no(frame);
    frame_no = sixel_frame_get_frame_no(frame);
    if (loop_no > context->highest_loop_no) {
        context->highest_loop_no = loop_no;
    }
    if (frame_no > context->highest_frame_no) {
        context->highest_frame_no = frame_no;
    }
    if (sixel_frame_get_multiframe(frame) != 0) {
        context->saw_multiframe = 1;
    }

    if (loop_no >= context->required_loop_no) {
        context->reached_required_loop = 1;
        return SIXEL_INTERRUPTED;
    }
    if (context->callback_count >= context->max_callbacks) {
        return SIXEL_INTERRUPTED;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
capture_gif_sequence_probe(sixel_frame_t *frame, void *data)
{
    gif_sequence_probe_context_t *context;
    int loop_no;
    int frame_no;
    int expected_loop_no;
    int expected_frame_no;
    int expected_index;

    context = (gif_sequence_probe_context_t *)data;
    loop_no = 0;
    frame_no = 0;
    expected_loop_no = 0;
    expected_frame_no = 0;
    expected_index = 0;
    if (context == NULL ||
        frame == NULL ||
        context->expected_sequence == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    loop_no = sixel_frame_get_loop_no(frame);
    frame_no = sixel_frame_get_frame_no(frame);
    if (sixel_frame_get_multiframe(frame) != 0) {
        context->saw_multiframe = 1;
    }
    if (context->callback_count >= context->expected_count) {
        context->mismatch_index = context->callback_count;
        context->mismatch_loop_no = loop_no;
        context->mismatch_frame_no = frame_no;
        return SIXEL_BAD_INPUT;
    }

    expected_index = context->callback_count * 2;
    expected_loop_no = context->expected_sequence[expected_index + 0];
    expected_frame_no = context->expected_sequence[expected_index + 1];
    if (loop_no != expected_loop_no || frame_no != expected_frame_no) {
        context->mismatch_index = context->callback_count;
        context->mismatch_loop_no = loop_no;
        context->mismatch_frame_no = frame_no;
        return SIXEL_BAD_INPUT;
    }

    context->callback_count += 1;
    return SIXEL_OK;
}

static float
srgb_from_linear(float linear_value)
{
    if (linear_value <= 0.0031308f) {
        return linear_value * 12.92f;
    }
    return 1.055f * powf(linear_value, 1.0f / 2.4f) - 0.055f;
}

static int
float_approx_equal(float left, float right, float tolerance)
{
    return fabsf(left - right) <= tolerance;
}

static int
loader_test_setenv(char const *name, char const *value)
{
    if (name == NULL || name[0] == '\0' || value == NULL) {
        return -1;
    }
#if defined(_MSC_VER)
    return _putenv_s(name, value);
#elif defined(_WIN32) && defined(HAVE__PUTENV_S) && HAVE__PUTENV_S
    /* MinGW can provide _putenv_s without defining _MSC_VER. */
    return _putenv_s(name, value);
#else
# if defined(HAVE_SETENV)
    extern int setenv(char const *name, char const *value, int overwrite);

    return setenv(name, value, 1);
# else
    (void)name;
    (void)value;

    return -1;
# endif
#endif
}

/*
 * Configure HDR loader env values used by numeric probes.
 *
 * These tests toggle the same env tuple repeatedly. Keeping this in one
 * helper makes defaults explicit and avoids inconsistent setup across cases.
 */
static int
hdr_test_configure_loader_env(char const *fallback_profile,
                              char const *exposure_ev,
                              char const *tonemap,
                              char const *use_header_exposure)
{
    if (fallback_profile == NULL || fallback_profile[0] == '\0' ||
        exposure_ev == NULL || exposure_ev[0] == '\0' ||
        tonemap == NULL || tonemap[0] == '\0' ||
        use_header_exposure == NULL || use_header_exposure[0] == '\0') {
        return 1;
    }
    if (loader_test_setenv("SIXEL_LOADER_HDR_FALLBACK_PROFILE",
                           fallback_profile) != 0 ||
        loader_test_setenv("SIXEL_LOADER_HDR_EXPOSURE_EV",
                           exposure_ev) != 0 ||
        loader_test_setenv("SIXEL_LOADER_HDR_TONEMAP",
                           tonemap) != 0 ||
        loader_test_setenv("SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE",
                           use_header_exposure) != 0) {
        return 1;
    }
    return 0;
}

static int
hdr_test_configure_loader_env_default(char const *use_header_exposure)
{
    return hdr_test_configure_loader_env("linear-srgb",
                                         "0",
                                         "none",
                                         use_header_exposure);
}

static int
hdr_test_is_linear_srgb(double gamma,
                        double white_x,
                        double white_y,
                        double red_x,
                        double red_y,
                        double green_x,
                        double green_y,
                        double blue_x,
                        double blue_y)
{
    double const gamma_epsilon = 0.000001;
    double const chroma_epsilon = 0.0001;

    if (fabs(gamma - 1.0) > gamma_epsilon) {
        return 0;
    }
    if (fabs(white_x - HDR_TEST_SRGB_WHITE_X) > chroma_epsilon) {
        return 0;
    }
    if (fabs(white_y - HDR_TEST_SRGB_WHITE_Y) > chroma_epsilon) {
        return 0;
    }
    if (fabs(red_x - HDR_TEST_SRGB_RED_X) > chroma_epsilon) {
        return 0;
    }
    if (fabs(red_y - HDR_TEST_SRGB_RED_Y) > chroma_epsilon) {
        return 0;
    }
    if (fabs(green_x - HDR_TEST_SRGB_GREEN_X) > chroma_epsilon) {
        return 0;
    }
    if (fabs(green_y - HDR_TEST_SRGB_GREEN_Y) > chroma_epsilon) {
        return 0;
    }
    if (fabs(blue_x - HDR_TEST_SRGB_BLUE_X) > chroma_epsilon) {
        return 0;
    }
    if (fabs(blue_y - HDR_TEST_SRGB_BLUE_Y) > chroma_epsilon) {
        return 0;
    }
    return 1;
}

static float
srgb_from_linear_unit_clamped(float linear_value)
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

static void
hdr_test_apply_dynamic_range(float *rgb,
                             double exposure_ev,
                             hdr_test_tonemap_mode_t tonemap_mode)
{
    int channel;
    double value;
    double exposure_scale;

    if (rgb == NULL) {
        return;
    }
    if (tonemap_mode == HDR_TEST_TONEMAP_NONE &&
        fabs(exposure_ev) <= 0.0000001) {
        return;
    }

    exposure_scale = pow(2.0, exposure_ev);
    if (!isfinite(exposure_scale) || exposure_scale <= 0.0) {
        exposure_scale = 1.0;
    }

    for (channel = 0; channel < 3; ++channel) {
        value = (double)rgb[channel];
        if (!isfinite(value) || value < 0.0) {
            value = 0.0;
        }
        value *= exposure_scale;
        if (!isfinite(value)) {
            if (tonemap_mode == HDR_TEST_TONEMAP_REINHARD) {
                value = DBL_MAX;
            } else {
                value = (double)FLT_MAX;
            }
        }
        if (value < 0.0) {
            value = 0.0;
        } else if (tonemap_mode == HDR_TEST_TONEMAP_NONE &&
                   value > (double)FLT_MAX) {
            value = (double)FLT_MAX;
        }

        if (tonemap_mode == HDR_TEST_TONEMAP_REINHARD) {
            value = value / (1.0 + value);
            if (!isfinite(value) || value < 0.0) {
                value = 0.0;
            } else if (value > 1.0) {
                value = 1.0;
            }
        }
        rgb[channel] = (float)value;
    }
}

static void
hdr_test_apply_source_profile(float *rgb,
                              hdr_test_gamma_mode_t gamma_mode,
                              hdr_test_primaries_mode_t primaries_mode,
                              hdr_test_fallback_mode_t fallback_mode)
{
    double effective_gamma;
    double effective_white_x;
    double effective_white_y;
    double effective_red_x;
    double effective_red_y;
    double effective_green_x;
    double effective_green_y;
    double effective_blue_x;
    double effective_blue_y;
    int has_header_hint;
    int header_linear_override;
    int converted;
    sixel_cms_profile_t *src_profile;

    if (rgb == NULL) {
        return;
    }

    effective_gamma = 1.0;
    effective_white_x = HDR_TEST_SRGB_WHITE_X;
    effective_white_y = HDR_TEST_SRGB_WHITE_Y;
    effective_red_x = HDR_TEST_SRGB_RED_X;
    effective_red_y = HDR_TEST_SRGB_RED_Y;
    effective_green_x = HDR_TEST_SRGB_GREEN_X;
    effective_green_y = HDR_TEST_SRGB_GREEN_Y;
    effective_blue_x = HDR_TEST_SRGB_BLUE_X;
    effective_blue_y = HDR_TEST_SRGB_BLUE_Y;
    has_header_hint = 0;
    header_linear_override = 0;
    converted = 0;
    src_profile = NULL;

    if (gamma_mode == HDR_TEST_GAMMA_22) {
        has_header_hint = 1;
        effective_gamma = 2.2;
    }
    if (primaries_mode == HDR_TEST_PRIMARIES_BT2020) {
        has_header_hint = 1;
        effective_red_x = HDR_TEST_BT2020_RED_X;
        effective_red_y = HDR_TEST_BT2020_RED_Y;
        effective_green_x = HDR_TEST_BT2020_GREEN_X;
        effective_green_y = HDR_TEST_BT2020_GREEN_Y;
        effective_blue_x = HDR_TEST_BT2020_BLUE_X;
        effective_blue_y = HDR_TEST_BT2020_BLUE_Y;
    }

    if (has_header_hint) {
        if (hdr_test_is_linear_srgb(effective_gamma,
                                    effective_white_x,
                                    effective_white_y,
                                    effective_red_x,
                                    effective_red_y,
                                    effective_green_x,
                                    effective_green_y,
                                    effective_blue_x,
                                    effective_blue_y)) {
            header_linear_override = 1;
        } else {
            src_profile = sixel_cms_create_rgb_profile_from_gamma_chrm(
                effective_gamma,
                effective_white_x,
                effective_white_y,
                effective_red_x,
                effective_red_y,
                effective_green_x,
                effective_green_y,
                effective_blue_x,
                effective_blue_y);
        }
    }

    if (src_profile == NULL && !header_linear_override) {
        if (fallback_mode == HDR_TEST_FALLBACK_SRGB) {
            src_profile = sixel_cms_create_srgb_profile();
        }
    }

    if (src_profile != NULL) {
        converted = sixel_cms_convert_profile_to_linearrgb(
            (unsigned char *)rgb,
            1,
            1,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
            src_profile);
        sixel_cms_close_profile(src_profile);
        if (!converted) {
            rgb[0] = 0.5f;
            rgb[1] = 0.25f;
            rgb[2] = 0.125f;
        }
    }
}

static int
hdr_test_expected_first_pixel(float out_rgb[3],
                              hdr_test_gamma_mode_t gamma_mode,
                              hdr_test_primaries_mode_t primaries_mode,
                              double exposure_ev,
                              hdr_test_tonemap_mode_t tonemap_mode,
                              int cms_engine,
                              hdr_test_fallback_mode_t fallback_mode,
                              int target_pixelformat)
{
    if (out_rgb == NULL) {
        return 1;
    }

    out_rgb[0] = 0.5f;
    out_rgb[1] = 0.25f;
    out_rgb[2] = 0.125f;

    if (cms_engine != SIXEL_CMS_ENGINE_NONE) {
        hdr_test_apply_source_profile(out_rgb,
                                      gamma_mode,
                                      primaries_mode,
                                      fallback_mode);
    }
    hdr_test_apply_dynamic_range(out_rgb, exposure_ev, tonemap_mode);

    if (cms_engine != SIXEL_CMS_ENGINE_NONE &&
        target_pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32) {
        out_rgb[0] = srgb_from_linear_unit_clamped(out_rgb[0]);
        out_rgb[1] = srgb_from_linear_unit_clamped(out_rgb[1]);
        out_rgb[2] = srgb_from_linear_unit_clamped(out_rgb[2]);
    }

    return 0;
}

/*
 * Resolve environment variables through the project compatibility wrapper.
 * This keeps the test runner free of MSVC's deprecated getenv() diagnostic.
 */
static char const *
loader_test_getenv(char const *name)
{
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    return sixel_compat_getenv(name);
}

static char const *
resolve_source_root_for_pixelformat_test(void)
{
    char const *source_root;

    source_root = loader_test_getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = loader_test_getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = loader_test_getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

    return source_root;
}

typedef struct builtin_loader_probe_options {
    int require_static;
    int use_palette;
    int reqcolors;
    int set_bgcolor;
    unsigned char const *bgcolor;
    int set_loop_control;
    int loop_control;
    int set_cms_engine;
    int cms_engine;
    int set_prefer_float32;
    int prefer_float32;
} builtin_loader_probe_options_t;

static void
init_builtin_loader_probe_options(builtin_loader_probe_options_t *options)
{
    if (options == NULL) {
        return;
    }

    /*
     * Keep every optional setopt gate initialized here.  Many focused loader
     * tests override only the fields they exercise, so a new option field must
     * not make older probe callers pass indeterminate bytes into setopt.
     */
    memset(options, 0, sizeof(*options));
    options->reqcolors = 256;
    options->loop_control = SIXEL_LOOP_AUTO;
    options->cms_engine = SIXEL_CMS_ENGINE_NONE;
}

static int
run_builtin_loader_probe_case(char const *label,
                              char const *relative_path,
                              builtin_loader_probe_options_t const *options,
                              sixel_load_image_function callback,
                              void *callback_context,
                              SIXELSTATUS *load_status_out)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_callback_state_t callback_state;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    int require_static;
    int use_palette;
    int reqcolors;
    int set_bgcolor;
    unsigned char const *bgcolor;
    int loop_control;
    int cms_engine;
    int prefer_float32;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    source_root = NULL;
    cancel_flag = 0;
    require_static = 0;
    use_palette = 0;
    reqcolors = 256;
    set_bgcolor = 0;
    bgcolor = NULL;
    loop_control = SIXEL_LOOP_AUTO;
    cms_engine = SIXEL_CMS_ENGINE_NONE;
    prefer_float32 = 0;
    result = 1;
    if (load_status_out != NULL) {
        *load_status_out = SIXEL_FALSE;
    }

    if (label == NULL ||
        relative_path == NULL ||
        options == NULL ||
        callback == NULL) {
        return 1;
    }

    source_root = resolve_source_root_for_pixelformat_test();
    if (build_image_path(source_root,
                         relative_path,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "%s: failed to build image path\n", label);
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator initialization failed\n", label);
        return 1;
    }

    status = sixel_chunk_create_from_source(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        goto cleanup;
    }

    status = new_builtin_component_for_pixelformat_test(allocator, (void **)&component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: component init failed (%d)\n", label, (int)status);
        goto cleanup;
    }

    require_static = options->require_static;
    use_palette = options->use_palette;
    reqcolors = options->reqcolors;
    set_bgcolor = options->set_bgcolor;
    bgcolor = options->bgcolor;
    loop_control = options->loop_control;
    cms_engine = options->cms_engine;
    prefer_float32 = options->prefer_float32;

    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &require_static);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &use_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    if (set_bgcolor != 0) {
        status = sixel_loader_component_setopt(component,
                                               SIXEL_LOADER_OPTION_BGCOLOR,
                                               bgcolor);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    if (options->set_loop_control != 0) {
        status = sixel_loader_component_setopt(component,
                                               SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                               &loop_control);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    if (options->set_cms_engine != 0) {
        status = sixel_loader_component_setopt(
            component,
            SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE,
            &cms_engine);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }
    if (options->set_prefer_float32 != 0) {
        status = sixel_loader_component_setopt(
            component,
            SIXEL_LOADER_COMPONENT_OPTION_PREFER_FLOAT32,
            &prefer_float32);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
    }

    callback_state.loader = NULL;
    callback_state.fn = callback;
    callback_state.context = callback_context;
    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (load_status_out != NULL) {
        *load_status_out = status;
    }
    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    return result;
}
