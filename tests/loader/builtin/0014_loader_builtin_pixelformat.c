/*
 * Verify builtin loader reports expected pixelformats:
 * - RGBA(8-bit)  -> RGBA8888 (default alpha keycolor path keeps alpha)
 * - GIF(opaque, palette on) -> PAL8
 * - GIF(alpha, palette on)  -> PAL8
 * - GIF(opaque, palette off) -> RGB888
 * - GIF(alpha, palette off)  -> RGBA8888
 * - GIF(alpha, palette on, low reqcolors) -> RGBA8888 fallback
 * - GIF(alpha, palette on, low reqcolors + bgcolor) -> RGB888 fallback
 * - GIF(anim without NETSCAPE extension) reports multiframe metadata
 * - GIF(loop=auto/force) keeps unbounded NETSCAPE loop behavior
 * - HDR(RGBE) -> LINEARRGBFLOAT32
 * - Gray(16-bit) -> RGBFLOAT32 (no 8-bit precision loss)
 * - PSD RGB16/RGB32 callbacks keep float precision (RGBFLOAT32)
 * - PSD CMYK32/Lab32 callbacks keep float precision family
 * - PSD RGB8+alpha callback stays RGB888 (mask side-channel handles alpha)
 */

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "tests/loader/pixelformat_test_common.h"
#include "src/cms.h"
#include "src/frompsd.h"
#include "src/loader-common.h"

static SIXELSTATUS
new_builtin_component_for_pixelformat_test(sixel_allocator_t *allocator,
                                           sixel_loader_component_t **ppcomponent)
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

    context = (hdr_numeric_probe_context_t *)data;
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

    pixels = sixel_frame_get_pixels_float32(frame);
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

#if defined(_MSC_VER)
#define TEST_GETENV_CACHE_SLOTS 16
static char *test_getenv_cache[TEST_GETENV_CACHE_SLOTS];
static size_t test_getenv_cache_cursor;
#endif

/*
 * Resolve environment variables without relying on private compat helpers.
 *
 * MSVC marks getenv() as deprecated and this test is compiled with /WX.
 * Keep a small rotating cache for values duplicated via _dupenv_s() so
 * callers can treat the return value like getenv() for the test lifetime.
 */
static char const *
loader_test_getenv(char const *name)
{
#if defined(_MSC_VER)
    char *value;
    size_t value_length;
    errno_t error_code;
    size_t slot;

    value = NULL;
    value_length = 0u;
    error_code = 0;
    slot = 0u;

    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    error_code = _dupenv_s(&value, &value_length, name);
    if (error_code != 0 || value == NULL || value_length == 0u) {
        free(value);
        return NULL;
    }

    slot = test_getenv_cache_cursor % TEST_GETENV_CACHE_SLOTS;
    free(test_getenv_cache[slot]);
    test_getenv_cache[slot] = value;
    test_getenv_cache_cursor += 1u;

    return test_getenv_cache[slot];
#else
    if (name == NULL || name[0] == '\0') {
        return NULL;
    }
    return getenv(name);
#endif
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
    int set_loop_control;
    int loop_control;
    int set_cms_engine;
    int cms_engine;
} builtin_loader_probe_options_t;

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
    int loop_control;
    int cms_engine;
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
    loop_control = SIXEL_LOOP_AUTO;
    cms_engine = SIXEL_CMS_ENGINE_NONE;
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

    status = sixel_chunk_new(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        goto cleanup;
    }

    status = new_builtin_component_for_pixelformat_test(allocator, &component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: component init failed (%d)\n", label, (int)status);
        goto cleanup;
    }

    require_static = options->require_static;
    use_palette = options->use_palette;
    reqcolors = options->reqcolors;
    loop_control = options->loop_control;
    cms_engine = options->cms_engine;

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
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
    return result;
}

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

    memset(context, 0, sizeof(*context));
    options.require_static = 1;
    options.use_palette = 0;
    options.reqcolors = 256;
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
        { 1.0f, 0.5f, 0.25f },
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
        { 4.0f, 2.0f, 1.0f },
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

static int
run_builtin_loader_hdr_gamma_numeric_test(void)
{
    char const *sample_path;
    hdr_numeric_probe_context_t linear_probe;
    hdr_numeric_probe_context_t gamma_probe;
    float expected_linear[3];
    float expected_gamma[3];
    float tolerance;
    int index;
    int result;

    sample_path = "/tests/data/inputs/formats/stbi_midtones.hdr";
    tolerance = 0.0005f;
    expected_linear[0] = 0.5f;
    expected_linear[1] = 0.25f;
    expected_linear[2] = 0.125f;

    if (loader_cms_target_pixelformat() != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        fprintf(stderr,
                "builtin loader hdr numeric gamma: expected target RGBFLOAT32\n");
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin loader hdr numeric cms=off",
        sample_path,
        SIXEL_CMS_ENGINE_NONE,
        &linear_probe);
    if (result != 0) {
        return result;
    }
    if (linear_probe.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        linear_probe.colorspace != SIXEL_COLORSPACE_LINEAR) {
        fprintf(stderr,
                "builtin loader hdr numeric cms=off: unexpected frame contract "
                "(pixelformat=%d colorspace=%d)\n",
                linear_probe.pixelformat,
                linear_probe.colorspace);
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        if (!float_approx_equal(linear_probe.first_pixel[index],
                                expected_linear[index],
                                tolerance)) {
            fprintf(stderr,
                    "builtin loader hdr numeric cms=off: channel %d mismatch "
                    "(actual=%f expected=%f)\n",
                    index,
                    linear_probe.first_pixel[index],
                    expected_linear[index]);
            return 1;
        }
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin loader hdr numeric cms=on",
        sample_path,
        SIXEL_CMS_ENGINE_AUTO,
        &gamma_probe);
    if (result != 0) {
        return result;
    }
    if (gamma_probe.pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
        gamma_probe.colorspace != SIXEL_COLORSPACE_GAMMA) {
        fprintf(stderr,
                "builtin loader hdr numeric cms=on: unexpected frame contract "
                "(pixelformat=%d colorspace=%d)\n",
                gamma_probe.pixelformat,
                gamma_probe.colorspace);
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        expected_gamma[index] = srgb_from_linear(expected_linear[index]);
        if (!float_approx_equal(gamma_probe.first_pixel[index],
                                expected_gamma[index],
                                tolerance)) {
            fprintf(stderr,
                    "builtin loader hdr numeric cms=on: channel %d mismatch "
                    "(actual=%f expected=%f)\n",
                    index,
                    gamma_probe.first_pixel[index],
                    expected_gamma[index]);
            return 1;
        }
    }

    if (fabsf(gamma_probe.first_pixel[1] - linear_probe.first_pixel[1]) < 0.1f) {
        fprintf(stderr,
                "builtin loader hdr numeric cms=on: gamma conversion not observed\n");
        return 1;
    }

    return 0;
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
    char const *sample_path;
    hdr_numeric_probe_context_t linear_probe;
    hdr_numeric_probe_context_t cms_probe;
    float expected_linear[3];
    float expected_cms[3];
    float tolerance;
    int index;
    int result;

    sample_path = "/tests/data/inputs/formats/stbi_midtones.hdr";
    tolerance = 0.0007f;
    expected_linear[0] = 0.5f;
    expected_linear[1] = 0.25f;
    expected_linear[2] = 0.125f;

    if (loader_cms_target_pixelformat() !=
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        fprintf(stderr,
                "builtin loader hdr fallback profile numeric: expected "
                "target LINEARRGBFLOAT32\n");
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin loader hdr fallback numeric cms=off",
        sample_path,
        SIXEL_CMS_ENGINE_NONE,
        &linear_probe);
    if (result != 0) {
        return result;
    }
    if (linear_probe.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        linear_probe.colorspace != SIXEL_COLORSPACE_LINEAR) {
        fprintf(stderr,
                "builtin loader hdr fallback numeric cms=off: unexpected "
                "frame contract (pixelformat=%d colorspace=%d)\n",
                linear_probe.pixelformat,
                linear_probe.colorspace);
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        if (!float_approx_equal(linear_probe.first_pixel[index],
                                expected_linear[index],
                                tolerance)) {
            fprintf(stderr,
                    "builtin loader hdr fallback numeric cms=off: channel %d "
                    "mismatch (actual=%f expected=%f)\n",
                    index,
                    linear_probe.first_pixel[index],
                    expected_linear[index]);
            return 1;
        }
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin loader hdr fallback numeric cms=on",
        sample_path,
        SIXEL_CMS_ENGINE_BUILTIN,
        &cms_probe);
    if (result != 0) {
        return result;
    }
    if (cms_probe.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        cms_probe.colorspace != SIXEL_COLORSPACE_LINEAR) {
        fprintf(stderr,
                "builtin loader hdr fallback numeric cms=on: unexpected frame "
                "contract (pixelformat=%d colorspace=%d)\n",
                cms_probe.pixelformat,
                cms_probe.colorspace);
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        expected_cms[index] = expected_linear[index];
    }
    if (!compute_hdr_srgb_fallback_expected(expected_cms)) {
        fprintf(stderr,
                "builtin loader hdr fallback numeric: failed to compute "
                "expected srgb fallback conversion\n");
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        if (!float_approx_equal(cms_probe.first_pixel[index],
                                expected_cms[index],
                                tolerance)) {
            fprintf(stderr,
                    "builtin loader hdr fallback numeric cms=on: channel %d "
                    "mismatch (actual=%f expected=%f)\n",
                    index,
                    cms_probe.first_pixel[index],
                    expected_cms[index]);
            return 1;
        }
    }

    if (fabsf(expected_cms[0] - expected_linear[0]) > 0.05f &&
        fabsf(cms_probe.first_pixel[0] - linear_probe.first_pixel[0]) < 0.05f) {
        fprintf(stderr,
                "builtin loader hdr fallback numeric cms=on: source profile "
                "conversion not observed\n");
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_hdr_header_priority_numeric_test(void)
{
    char const *sample_path;
    hdr_numeric_probe_context_t linear_probe;
    hdr_numeric_probe_context_t cms_probe;
    float expected_linear[3];
    float tolerance;
    int index;
    int result;

    sample_path = "/tests/data/inputs/formats/stbi_midtones_hdrmeta_linear.hdr";
    tolerance = 0.0007f;
    expected_linear[0] = 0.5f;
    expected_linear[1] = 0.25f;
    expected_linear[2] = 0.125f;

    if (loader_cms_target_pixelformat() !=
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        fprintf(stderr,
                "builtin loader hdr header-priority numeric: expected target "
                "LINEARRGBFLOAT32\n");
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin loader hdr header-priority numeric cms=off",
        sample_path,
        SIXEL_CMS_ENGINE_NONE,
        &linear_probe);
    if (result != 0) {
        return result;
    }
    if (linear_probe.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        linear_probe.colorspace != SIXEL_COLORSPACE_LINEAR) {
        fprintf(stderr,
                "builtin loader hdr header-priority numeric cms=off: "
                "unexpected frame contract (pixelformat=%d colorspace=%d)\n",
                linear_probe.pixelformat,
                linear_probe.colorspace);
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        if (!float_approx_equal(linear_probe.first_pixel[index],
                                expected_linear[index],
                                tolerance)) {
            fprintf(stderr,
                    "builtin loader hdr header-priority numeric cms=off: "
                    "channel %d mismatch (actual=%f expected=%f)\n",
                    index,
                    linear_probe.first_pixel[index],
                    expected_linear[index]);
            return 1;
        }
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin loader hdr header-priority numeric cms=on",
        sample_path,
        SIXEL_CMS_ENGINE_BUILTIN,
        &cms_probe);
    if (result != 0) {
        return result;
    }
    if (cms_probe.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        cms_probe.colorspace != SIXEL_COLORSPACE_LINEAR) {
        fprintf(stderr,
                "builtin loader hdr header-priority numeric cms=on: "
                "unexpected frame contract (pixelformat=%d colorspace=%d)\n",
                cms_probe.pixelformat,
                cms_probe.colorspace);
        return 1;
    }

    for (index = 0; index < 3; ++index) {
        if (!float_approx_equal(cms_probe.first_pixel[index],
                                expected_linear[index],
                                tolerance)) {
            fprintf(stderr,
                    "builtin loader hdr header-priority numeric cms=on: "
                    "channel %d mismatch (actual=%f expected=%f)\n",
                    index,
                    cms_probe.first_pixel[index],
                    expected_linear[index]);
            return 1;
        }
    }

    if (fabsf(cms_probe.first_pixel[0] - linear_probe.first_pixel[0]) >
        tolerance) {
        fprintf(stderr,
                "builtin loader hdr header-priority numeric cms=on: "
                "unexpected fallback-profile override observed\n");
        return 1;
    }

    return 0;
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
        &hdr_numeric_static_cases[HDR_NUMERIC_STATIC_CASE_HEADER_EXPOSURE_MULTI]);
}

static int
run_builtin_loader_hdr_header_exposure_disabled_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[
            HDR_NUMERIC_STATIC_CASE_HEADER_EXPOSURE_DISABLED]);
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
run_builtin_loader_hdr_gamma_invalid_primaries_valid_numeric_test(void)
{
    return run_builtin_loader_hdr_partial_header_numeric_test(
        "builtin loader hdr gamma invalid + primaries valid numeric",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_gamma_invalid_bt2020.hdr",
        HDR_TEST_GAMMA_NONE,
        HDR_TEST_PRIMARIES_BT2020);
}

static int
run_builtin_loader_hdr_primaries_invalid_gamma_valid_numeric_test(void)
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

static int
run_builtin_loader_hdr_invalid_header_exposure_numeric_test(void)
{
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t invalid_probe;
    int result;

    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid header exposure baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &baseline_probe);
    if (result != 0) {
        return result;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid header exposure",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_exposure_invalid_zero.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &invalid_probe);
    if (result != 0) {
        return result;
    }

    return hdr_test_compare_probe("builtin hdr invalid header exposure",
                                  &invalid_probe,
                                  &baseline_probe,
                                  0.0007f);
}

static int
run_builtin_loader_hdr_mixed_header_exposure_invalid_numeric_test(void)
{
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t mixed_probe;
    int result;

    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr mixed header exposure invalid baseline",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &baseline_probe);
    if (result != 0) {
        return result;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr mixed header exposure invalid",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_exposure2_invalid_overflow.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &mixed_probe);
    if (result != 0) {
        return result;
    }

    return hdr_test_compare_probe("builtin hdr mixed header exposure invalid",
                                  &mixed_probe,
                                  &baseline_probe,
                                  0.0007f);
}

static int
run_builtin_loader_hdr_invalid_use_hdr_exposure_env_test(void)
{
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t invalid_probe;
    int result;

    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid use-header-exposure env baseline",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &baseline_probe);
    if (result != 0) {
        return result;
    }

    if (loader_test_setenv("SIXEL_LOADER_HDR_USE_HEADER_EXPOSURE",
                           "invalid-value") != 0) {
        return 1;
    }
    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid use-header-exposure env",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_exposure2.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &invalid_probe);
    if (result != 0) {
        return result;
    }

    return hdr_test_compare_probe(
        "builtin hdr invalid use-header-exposure env",
        &invalid_probe,
        &baseline_probe,
        0.0007f);
}

static int
run_builtin_loader_hdr_duplicate_header_metadata_numeric_test(void)
{
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t duplicate_probe;
    int result;

    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr duplicate metadata baseline",
        "/tests/data/inputs/formats/stbi_midtones_hdrmeta_gamma22_bt2020.hdr",
        SIXEL_CMS_ENGINE_AUTO,
        &baseline_probe);
    if (result != 0) {
        return result;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr duplicate metadata",
        "/tests/data/inputs/formats/"
        "stbi_midtones_hdrmeta_duplicate_gamma_primaries_last_wins.hdr",
        SIXEL_CMS_ENGINE_AUTO,
        &duplicate_probe);
    if (result != 0) {
        return result;
    }

    return hdr_test_compare_probe("builtin hdr duplicate metadata",
                                  &duplicate_probe,
                                  &baseline_probe,
                                  0.0012f);
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
    return "/tests/data/inputs/formats/stbi_midtones_hdrmeta_gamma22_bt2020.hdr";
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
                "%s: frame contract mismatch (actual pf=%d cs=%d, expected pf=%d cs=%d)\n",
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
hdr_test_parse_gamma_mode(char const *text,
                          hdr_test_gamma_mode_t *out_mode)
{
    if (out_mode == NULL) {
        return 1;
    }
    if (text == NULL || text[0] == '\0' || strcmp(text, "none") == 0) {
        *out_mode = HDR_TEST_GAMMA_NONE;
        return 0;
    }
    if (strcmp(text, "2.2") == 0 || strcmp(text, "2_2") == 0) {
        *out_mode = HDR_TEST_GAMMA_22;
        return 0;
    }
    return 1;
}

static int
hdr_test_parse_primaries_mode(char const *text,
                              hdr_test_primaries_mode_t *out_mode)
{
    if (out_mode == NULL) {
        return 1;
    }
    if (text == NULL || text[0] == '\0' || strcmp(text, "none") == 0) {
        *out_mode = HDR_TEST_PRIMARIES_NONE;
        return 0;
    }
    if (strcmp(text, "bt2020") == 0) {
        *out_mode = HDR_TEST_PRIMARIES_BT2020;
        return 0;
    }
    return 1;
}

static int
hdr_test_parse_tonemap_mode(char const *text,
                            hdr_test_tonemap_mode_t *out_mode)
{
    if (out_mode == NULL) {
        return 1;
    }
    if (text == NULL || text[0] == '\0' || strcmp(text, "none") == 0) {
        *out_mode = HDR_TEST_TONEMAP_NONE;
        return 0;
    }
    if (strcmp(text, "reinhard") == 0) {
        *out_mode = HDR_TEST_TONEMAP_REINHARD;
        return 0;
    }
    return 1;
}

static int
hdr_test_parse_fallback_mode(char const *text,
                             hdr_test_fallback_mode_t *out_mode)
{
    if (out_mode == NULL) {
        return 1;
    }
    if (text == NULL || text[0] == '\0' ||
        strcmp(text, "linear-srgb") == 0) {
        *out_mode = HDR_TEST_FALLBACK_LINEAR_SRGB;
        return 0;
    }
    if (strcmp(text, "srgb") == 0) {
        *out_mode = HDR_TEST_FALLBACK_SRGB;
        return 0;
    }
    return 1;
}

static int
hdr_test_parse_cms_engine(char const *text, int *out_cms_engine)
{
    if (out_cms_engine == NULL) {
        return 1;
    }
    if (text == NULL || text[0] == '\0' || strcmp(text, "none") == 0) {
        *out_cms_engine = SIXEL_CMS_ENGINE_NONE;
        return 0;
    }
    if (strcmp(text, "auto") == 0) {
        *out_cms_engine = SIXEL_CMS_ENGINE_AUTO;
        return 0;
    }
    return 1;
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
                "builtin loader hdr single-case numeric: invalid case env values\n");
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
                "builtin loader hdr single-case numeric: failed to set loader env\n");
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
                "%s: frame contract mismatch (pf=%d expected=%d cs=%d expected=%d)\n",
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
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t invalid_probe;
    int result;

    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }

    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid fallback baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_AUTO,
        &baseline_probe);
    if (result != 0) {
        return result;
    }

    if (loader_test_setenv("SIXEL_LOADER_HDR_FALLBACK_PROFILE",
                           "invalid-profile") != 0) {
        return 1;
    }
    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid fallback",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_AUTO,
        &invalid_probe);
    if (result != 0) {
        return result;
    }
    return hdr_test_compare_probe("builtin hdr invalid fallback",
                                  &invalid_probe,
                                  &baseline_probe,
                                  0.0007f);
}

static int
run_builtin_loader_hdr_invalid_tonemap_numeric_test(void)
{
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t invalid_probe;
    int result;

    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }
    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid tonemap baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &baseline_probe);
    if (result != 0) {
        return result;
    }
    if (loader_test_setenv("SIXEL_LOADER_HDR_TONEMAP", "invalid-tonemap") != 0) {
        return 1;
    }
    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid tonemap",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &invalid_probe);
    if (result != 0) {
        return result;
    }
    return hdr_test_compare_probe("builtin hdr invalid tonemap",
                                  &invalid_probe,
                                  &baseline_probe,
                                  0.0007f);
}

static int
run_builtin_loader_hdr_invalid_exposure_numeric_test(void)
{
    hdr_numeric_probe_context_t baseline_probe;
    hdr_numeric_probe_context_t invalid_probe;
    int result;

    if (hdr_test_configure_loader_env_default("1") != 0) {
        return 1;
    }
    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid exposure baseline",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &baseline_probe);
    if (result != 0) {
        return result;
    }
    if (loader_test_setenv("SIXEL_LOADER_HDR_EXPOSURE_EV", "not-a-number") != 0) {
        return 1;
    }
    result = run_builtin_loader_hdr_numeric_probe_case(
        "builtin hdr invalid exposure",
        "/tests/data/inputs/formats/stbi_midtones.hdr",
        SIXEL_CMS_ENGINE_NONE,
        &invalid_probe);
    if (result != 0) {
        return result;
    }
    return hdr_test_compare_probe("builtin hdr invalid exposure",
                                  &invalid_probe,
                                  &baseline_probe,
                                  0.0007f);
}

static int
run_builtin_loader_gif_sequence_test(
    char const *label,
    char const *relative_path,
    int loop_control,
    int const *expected_sequence,
    int expected_count)
{
    SIXELSTATUS status;
    builtin_loader_probe_options_t options;
    gif_sequence_probe_context_t probe;
    int mismatch_expected_loop_no;
    int mismatch_expected_frame_no;
    int result;

    status = SIXEL_FALSE;
    mismatch_expected_loop_no = -1;
    mismatch_expected_frame_no = -1;
    result = 1;
    memset(&probe, 0, sizeof(probe));

    if (label == NULL ||
        relative_path == NULL ||
        expected_sequence == NULL ||
        expected_count <= 0) {
        return 1;
    }

    probe.callback_count = 0;
    probe.expected_count = expected_count;
    probe.mismatch_index = -1;
    probe.mismatch_loop_no = -1;
    probe.mismatch_frame_no = -1;
    probe.saw_multiframe = 0;
    probe.expected_sequence = expected_sequence;
    options.require_static = 0;
    options.use_palette = 1;
    options.reqcolors = 256;
    options.set_loop_control = 1;
    options.loop_control = loop_control;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;
    result = run_builtin_loader_probe_case(label,
                                           relative_path,
                                           &options,
                                           capture_gif_sequence_probe,
                                           &probe,
                                           &status);
    if (result != 0) {
        return result;
    }
    if (SIXEL_FAILED(status)) {
        if (probe.mismatch_index >= 0 &&
            probe.mismatch_index < expected_count) {
            mismatch_expected_loop_no =
                expected_sequence[probe.mismatch_index * 2 + 0];
            mismatch_expected_frame_no =
                expected_sequence[probe.mismatch_index * 2 + 1];
            fprintf(stderr,
                    "%s: sequence mismatch at %d "
                    "(actual=%d:%d expected=%d:%d)\n",
                    label,
                    probe.mismatch_index,
                    probe.mismatch_loop_no,
                    probe.mismatch_frame_no,
                    mismatch_expected_loop_no,
                    mismatch_expected_frame_no);
        } else {
            fprintf(stderr,
                    "%s: loader returned failure (%d)\n",
                    label,
                    (int)status);
        }
        return 1;
    }
    if (probe.callback_count != expected_count) {
        fprintf(stderr,
                "%s: callback count mismatch (actual=%d expected=%d)\n",
                label,
                probe.callback_count,
                expected_count);
        return 1;
    }
    if (probe.saw_multiframe == 0) {
        fprintf(stderr, "%s: frame metadata did not mark multiframe\n", label);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_gif_loop_disable_loop0_once_test(void)
{
    static int const expected_sequence[] = { 0, 0, 0, 1 };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=disable ignores loop0 and emits one pass",
        "/tests/data/inputs/formats/gif-anim-netscape-loop0.gif",
        SIXEL_LOOP_DISABLE,
        expected_sequence,
        2);
}

static int
run_builtin_loader_gif_loop_disable_loop1_once_test(void)
{
    static int const expected_sequence[] = { 0, 0, 0, 1 };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=disable ignores loop1 and emits one pass",
        "/tests/data/inputs/formats/gif-anim-netscape-loop1.gif",
        SIXEL_LOOP_DISABLE,
        expected_sequence,
        2);
}

static int
run_builtin_loader_gif_loop_disable_loop2_once_test(void)
{
    static int const expected_sequence[] = { 0, 0, 0, 1 };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=disable ignores loop2 and emits one pass",
        "/tests/data/inputs/formats/gif-anim-netscape-loop2.gif",
        SIXEL_LOOP_DISABLE,
        expected_sequence,
        2);
}

static int
run_builtin_loader_gif_loop_auto_loop1_once_test(void)
{
    static int const expected_sequence[] = { 0, 0, 0, 1 };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=auto respects loop1 as one pass",
        "/tests/data/inputs/formats/gif-anim-netscape-loop1.gif",
        SIXEL_LOOP_AUTO,
        expected_sequence,
        2);
}

static int
run_builtin_loader_gif_loop_auto_loop2_twice_test(void)
{
    static int const expected_sequence[] = {
        0, 0,
        0, 1,
        1, 0,
        1, 1
    };

    return run_builtin_loader_gif_sequence_test(
        "builtin gif loop=auto respects loop2 as two passes",
        "/tests/data/inputs/formats/gif-anim-netscape-loop2.gif",
        SIXEL_LOOP_AUTO,
        expected_sequence,
        4);
}

static int
run_builtin_loader_gif_unbounded_loop_probe_test(
    char const *label,
    char const *relative_path,
    int loop_control,
    int required_loop_no)
{
    SIXELSTATUS status;
    builtin_loader_probe_options_t options;
    gif_loop_probe_context_t probe;
    int result;

    status = SIXEL_FALSE;
    result = 1;
    memset(&probe, 0, sizeof(probe));

    if (label == NULL || relative_path == NULL || required_loop_no < 0) {
        return 1;
    }

    probe.callback_count = 0;
    /* Keep probe bounded while still allowing several full animation loops. */
    probe.max_callbacks = 64;
    probe.required_loop_no = required_loop_no;
    probe.highest_loop_no = -1;
    probe.highest_frame_no = -1;
    probe.saw_multiframe = 0;
    probe.reached_required_loop = 0;
    options.require_static = 0;
    options.use_palette = 1;
    options.reqcolors = 256;
    options.set_loop_control = 1;
    options.loop_control = loop_control;
    options.set_cms_engine = 0;
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;
    result = run_builtin_loader_probe_case(label,
                                           relative_path,
                                           &options,
                                           capture_gif_loop_probe_until_target,
                                           &probe,
                                           &status);
    if (result != 0) {
        return result;
    }
    if (status != SIXEL_INTERRUPTED) {
        fprintf(stderr,
                "%s: expected interruption status, got %d\n",
                label,
                (int)status);
        return 1;
    }
    if (probe.reached_required_loop == 0) {
        fprintf(stderr,
                "%s: loop threshold was not reached "
                "(required=%d highest=%d callbacks=%d)\n",
                label,
                required_loop_no,
                probe.highest_loop_no,
                probe.callback_count);
        return 1;
    }
    if (probe.saw_multiframe == 0) {
        fprintf(stderr, "%s: frame metadata did not mark multiframe\n", label);
        return 1;
    }

    return 0;
}

static int
run_builtin_loader_gif_loop_auto_loop0_unbounded_test(void)
{
    return run_builtin_loader_gif_unbounded_loop_probe_test(
        "builtin gif loop=auto loop0 remains unbounded",
        "/tests/data/inputs/formats/gif-anim-netscape-loop0.gif",
        SIXEL_LOOP_AUTO,
        2);
}

static int
run_builtin_loader_gif_loop_force_loop0_unbounded_test(void)
{
    return run_builtin_loader_gif_unbounded_loop_probe_test(
        "builtin gif loop=force loop0 remains unbounded",
        "/tests/data/inputs/formats/gif-anim-netscape-loop0.gif",
        SIXEL_LOOP_FORCE,
        2);
}

static int
run_builtin_loader_gif_loop_force_loop1_unbounded_test(void)
{
    return run_builtin_loader_gif_unbounded_loop_probe_test(
        "builtin gif loop=force ignores loop1 and stays unbounded",
        "/tests/data/inputs/formats/gif-anim-netscape-loop1.gif",
        SIXEL_LOOP_FORCE,
        2);
}

static int
run_builtin_loader_gif_loop_force_loop2_unbounded_test(void)
{
    return run_builtin_loader_gif_unbounded_loop_probe_test(
        "builtin gif loop=force ignores loop2 and stays unbounded",
        "/tests/data/inputs/formats/gif-anim-netscape-loop2.gif",
        SIXEL_LOOP_FORCE,
        3);
}

static int
run_builtin_loader_hdr_exposure_overflow_none_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[
            HDR_NUMERIC_STATIC_CASE_EXPOSURE_OVERFLOW_NONE]);
}

static int
run_builtin_loader_hdr_exposure_overflow_reinhard_numeric_test(void)
{
    return run_builtin_loader_hdr_static_numeric_case(
        &hdr_numeric_static_cases[
            HDR_NUMERIC_STATIC_CASE_EXPOSURE_OVERFLOW_REINHARD]);
}

static int
run_builtin_loader_hdr_case_with_cms(char const *label,
                                     int expected_pixelformat,
                                     int expected_colorspace,
                                     int cms_engine)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_context_t context;
    loader_probe_callback_state_t callback_state;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    int result;
    int require_static;
    int use_palette;
    int reqcolors;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    source_root = NULL;
    cancel_flag = 0;
    result = 1;
    require_static = 1;
    use_palette = 0;
    reqcolors = 256;

    source_root = resolve_source_root_for_pixelformat_test();

    if (build_image_path(source_root,
                         "/tests/data/inputs/formats/stbi_minimal.hdr",
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

    status = sixel_chunk_new(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: failed to read sample\n", label);
        goto cleanup;
    }

    status = new_builtin_component_for_pixelformat_test(allocator, &component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: component init failed (%d)\n", label, (int)status);
        goto cleanup;
    }

    context.callback_count = 0;
    context.pixelformat = 0;
    context.colorspace = 0;
    context.width = 0;
    context.height = 0;
    context.transparent = FRAME_METADATA_ANY;
    context.multiframe = FRAME_METADATA_ANY;
    callback_state.loader = NULL;
    callback_state.fn = capture_frame;
    callback_state.context = &context;

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
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE,
                                           &cms_engine);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "%s: loader reported failure (%d)\n",
                label,
                (int)status);
        goto cleanup;
    }

    if (context.callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch\n", label);
        goto cleanup;
    }
    if (context.pixelformat != expected_pixelformat) {
        fprintf(stderr,
                "%s: reported pixelformat %d\n",
                label,
                context.pixelformat);
        goto cleanup;
    }
    if (context.colorspace != expected_colorspace) {
        fprintf(stderr,
                "%s: reported colorspace %d\n",
                label,
                context.colorspace);
        goto cleanup;
    }
    if (context.width <= 0 || context.height <= 0) {
        fprintf(stderr,
                "%s: invalid geometry %dx%d\n",
                label,
                context.width,
                context.height);
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
    return result;
}

static int
run_builtin_loader_psd_validate_defensive_test(void)
{
    sixel_chunk_t chunk;
    sixel_builtin_psd_info_t info;
    unsigned char buffer[64];
    int decode_mode;
    int skip_icc_conversion;
    int colorspace;
    char message[128];
    int status;

    memset(&chunk, 0, sizeof(chunk));
    memset(&info, 0, sizeof(info));
    memset(buffer, 0, sizeof(buffer));
    memset(message, 0, sizeof(message));
    decode_mode = SIXEL_BUILTIN_PSD_DECODE_MODE_NONE;
    skip_icc_conversion = 0;
    colorspace = SIXEL_COLORSPACE_GAMMA;

    status = sixel_builtin_validate_psd_info(NULL,
                                             NULL,
                                             &decode_mode,
                                             &skip_icc_conversion,
                                             &colorspace,
                                             message,
                                             sizeof(message));
    if (status != SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED ||
        strcmp(message, "builtin PSD: malformed header/metadata") != 0) {
        fprintf(stderr,
                "builtin psd validate defensive: malformed header path mismatch "
                "(status=%d message=%s)\n",
                status,
                message);
        return 1;
    }

    chunk.buffer = buffer;
    chunk.size = sizeof(buffer);
    info.version = 1u;
    info.channels = 3u;
    info.width = 1u;
    info.height = 1u;
    info.depth = 8u;
    info.color_mode = 3u;
    info.compression = 0u;
    info.image_data_offset = chunk.size + 1u;
    message[0] = '\0';
    status = sixel_builtin_validate_psd_info(&chunk,
                                             &info,
                                             &decode_mode,
                                             &skip_icc_conversion,
                                             &colorspace,
                                             message,
                                             sizeof(message));
    if (status != SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED ||
        strcmp(message, "builtin PSD: malformed image data offset") != 0) {
        fprintf(stderr,
                "builtin psd validate defensive: image data offset mismatch "
                "(status=%d message=%s)\n",
                status,
                message);
        return 1;
    }

    if ((size_t)-1 <= 0xffffffffULL) {
        info.version = 1u;
        info.channels = 3u;
        info.width = 300000u;
        info.height = 300000u;
        info.depth = 32u;
        info.color_mode = 3u;
        info.compression = 0u;
        info.image_data_offset = 2u;
        message[0] = '\0';
        status = sixel_builtin_validate_psd_info(&chunk,
                                                 &info,
                                                 &decode_mode,
                                                 &skip_icc_conversion,
                                                 &colorspace,
                                                 message,
                                                 sizeof(message));
        if (status != SIXEL_BUILTIN_PSD_VALIDATE_MALFORMED ||
            strcmp(message, "builtin PSD: malformed dimensions/depth overflow")
                != 0) {
            fprintf(stderr,
                    "builtin psd validate defensive: overflow path mismatch "
                    "(status=%d message=%s)\n",
                    status,
                    message);
            return 1;
        }
    }

    return 0;
}

typedef int (*builtin_loader_env_test_fn_t)(void);

typedef struct builtin_loader_env_dispatch_entry {
    char const *env_name;
    builtin_loader_env_test_fn_t fn;
} builtin_loader_env_dispatch_entry_t;

typedef struct builtin_loader_env_dispatch_group {
    builtin_loader_env_dispatch_entry_t const *entries;
    size_t entry_count;
} builtin_loader_env_dispatch_group_t;

static int
run_builtin_loader_env_dispatch(
    builtin_loader_env_dispatch_entry_t const *entries,
    size_t entry_count)
{
    size_t index;
    char const *mode;

    index = 0u;
    mode = NULL;
    if (entries == NULL) {
        return -1;
    }

    for (index = 0u; index < entry_count; ++index) {
        mode = loader_test_getenv(entries[index].env_name);
        if (mode != NULL && strcmp(mode, "1") == 0) {
            return entries[index].fn();
        }
    }
    return -1;
}

static int
run_builtin_loader_env_dispatch_groups(
    builtin_loader_env_dispatch_group_t const *groups,
    size_t group_count)
{
    size_t group_index;
    int dispatch_result;

    group_index = 0u;
    dispatch_result = -1;
    if (groups == NULL) {
        return -1;
    }

    for (group_index = 0u; group_index < group_count; ++group_index) {
        dispatch_result = run_builtin_loader_env_dispatch(
            groups[group_index].entries,
            groups[group_index].entry_count);
        if (dispatch_result >= 0) {
            return dispatch_result;
        }
    }
    return -1;
}

static int
run_builtin_loader_test(void)
{
    static builtin_loader_env_dispatch_entry_t const hdr_env_dispatch[] = {
        { "SIXEL_TEST_HDR_NUMERIC_GAMMA",
          run_builtin_loader_hdr_gamma_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_FALLBACK_PROFILE",
          run_builtin_loader_hdr_fallback_profile_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_HEADER_PRIORITY",
          run_builtin_loader_hdr_header_priority_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_EXPOSURE",
          run_builtin_loader_hdr_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_TONEMAP_REINHARD",
          run_builtin_loader_hdr_tonemap_reinhard_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_SINGLE_CASE",
          run_builtin_loader_hdr_single_case_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_FALLBACK",
          run_builtin_loader_hdr_invalid_fallback_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_TONEMAP",
          run_builtin_loader_hdr_invalid_tonemap_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_EXPOSURE",
          run_builtin_loader_hdr_invalid_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_EXPOSURE_OVERFLOW_NONE",
          run_builtin_loader_hdr_exposure_overflow_none_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_EXPOSURE_OVERFLOW_REINHARD",
          run_builtin_loader_hdr_exposure_overflow_reinhard_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_HEADER_EXPOSURE",
          run_builtin_loader_hdr_header_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_HEADER_EXPOSURE_MULTI",
          run_builtin_loader_hdr_header_exposure_multi_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_HEADER_EXPOSURE_DISABLED",
          run_builtin_loader_hdr_header_exposure_disabled_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_GAMMA_INVALID_PRIMARIES_VALID",
          run_builtin_loader_hdr_gamma_invalid_primaries_valid_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_PRIMARIES_INVALID_GAMMA_VALID",
          run_builtin_loader_hdr_primaries_invalid_gamma_valid_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_HEADER_EXPOSURE",
          run_builtin_loader_hdr_invalid_header_exposure_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_MIXED_HEADER_EXPOSURE_INVALID",
          run_builtin_loader_hdr_mixed_header_exposure_invalid_numeric_test },
        { "SIXEL_TEST_HDR_NUMERIC_INVALID_USE_HEADER_EXPOSURE_ENV",
          run_builtin_loader_hdr_invalid_use_hdr_exposure_env_test },
        { "SIXEL_TEST_HDR_NUMERIC_DUPLICATE_HEADER_METADATA_LAST_WINS",
          run_builtin_loader_hdr_duplicate_header_metadata_numeric_test }
    };
    static builtin_loader_env_dispatch_entry_t const gif_env_dispatch[] = {
        { "SIXEL_TEST_GIF_LOOP_DISABLE_LOOP0_ONCE",
          run_builtin_loader_gif_loop_disable_loop0_once_test },
        { "SIXEL_TEST_GIF_LOOP_DISABLE_LOOP1_ONCE",
          run_builtin_loader_gif_loop_disable_loop1_once_test },
        { "SIXEL_TEST_GIF_LOOP_DISABLE_LOOP2_ONCE",
          run_builtin_loader_gif_loop_disable_loop2_once_test },
        { "SIXEL_TEST_GIF_LOOP_AUTO_LOOP1_ONCE",
          run_builtin_loader_gif_loop_auto_loop1_once_test },
        { "SIXEL_TEST_GIF_LOOP_AUTO_LOOP2_TWICE",
          run_builtin_loader_gif_loop_auto_loop2_twice_test },
        { "SIXEL_TEST_GIF_LOOP_AUTO_LOOP0_UNBOUNDED",
          run_builtin_loader_gif_loop_auto_loop0_unbounded_test },
        { "SIXEL_TEST_GIF_LOOP_FORCE_LOOP0_UNBOUNDED",
          run_builtin_loader_gif_loop_force_loop0_unbounded_test },
        { "SIXEL_TEST_GIF_LOOP_FORCE_LOOP1_UNBOUNDED",
          run_builtin_loader_gif_loop_force_loop1_unbounded_test },
        { "SIXEL_TEST_GIF_LOOP_FORCE_LOOP2_UNBOUNDED",
          run_builtin_loader_gif_loop_force_loop2_unbounded_test }
    };
    static builtin_loader_env_dispatch_entry_t const psd_env_dispatch[] = {
        { "SIXEL_TEST_PSD_VALIDATE_DEFENSIVE",
          run_builtin_loader_psd_validate_defensive_test }
    };
    static builtin_loader_env_dispatch_group_t const env_dispatch_groups[] = {
        {
            hdr_env_dispatch,
            sizeof(hdr_env_dispatch) / sizeof(hdr_env_dispatch[0])
        },
        {
            gif_env_dispatch,
            sizeof(gif_env_dispatch) / sizeof(gif_env_dispatch[0])
        },
        {
            psd_env_dispatch,
            sizeof(psd_env_dispatch) / sizeof(psd_env_dispatch[0])
        }
    };
    char const *expected_cms_pixelformat_text;
    unsigned char const bgcolor_white[3] = { 0xffu, 0xffu, 0xffu };
    int cms_target_pixelformat;
    int cms_target_colorspace;
    int parsed_pixelformat;
    int dispatch_result;
    int result;
    loader_component_case_spec_t psd_alpha_case;

    dispatch_result = run_builtin_loader_env_dispatch_groups(
        env_dispatch_groups,
        sizeof(env_dispatch_groups) / sizeof(env_dispatch_groups[0]));
    if (dispatch_result >= 0) {
        return dispatch_result;
    }

    result = run_loader_component_case("builtin loader rgba8",
                                       RGBA_IMAGE_PATH,
                                       SIXEL_PIXELFORMAT_RGBA8888,
                                       2,
                                       1,
                                       new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif opaque pal8",
        "/tests/data/inputs/small.gif",
        SIXEL_PIXELFORMAT_PAL8,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        1,
        -1,
        1,
        1,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent pal8",
        "/tests/data/inputs/formats/gif-transparent-static.gif",
        SIXEL_PIXELFORMAT_PAL8,
        8,
        8,
        1,
        FRAME_TRANSPARENT_NONNEG,
        0,
        1,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif opaque rgb",
        "/tests/data/inputs/small.gif",
        SIXEL_PIXELFORMAT_RGB888,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        1,
        -1,
        1,
        1,
        0,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent rgba",
        "/tests/data/inputs/formats/gif-transparent-static.gif",
        SIXEL_PIXELFORMAT_RGBA8888,
        8,
        8,
        1,
        -1,
        0,
        1,
        0,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent low-reqcolors rgba fallback",
        "/tests/data/inputs/formats/gif-transparent-static-3colors.gif",
        SIXEL_PIXELFORMAT_RGBA8888,
        8,
        8,
        1,
        -1,
        0,
        1,
        1,
        3,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent low-reqcolors rgb fallback with bgcolor",
        "/tests/data/inputs/formats/gif-transparent-static-3colors.gif",
        SIXEL_PIXELFORMAT_RGB888,
        8,
        8,
        1,
        -1,
        0,
        1,
        1,
        3,
        bgcolor_white,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif no-netscape multiframe metadata",
        "/tests/data/inputs/formats/gif-anim-no-netscape-2frame.gif",
        SIXEL_PIXELFORMAT_PAL8,
        6,
        6,
        2,
        -1,
        1,
        0,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif bgindex-oob fallback pal8",
        "/tests/data/inputs/formats/gif-bgindex-oob-anim.gif",
        SIXEL_PIXELFORMAT_PAL8,
        2,
        1,
        1,
        -1,
        1,
        1,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif netscape unknown subtype metadata",
        "/tests/data/inputs/formats/gif-anim-netscape-unknown-subtype.gif",
        SIXEL_PIXELFORMAT_PAL8,
        2,
        1,
        2,
        -1,
        1,
        0,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case_with_options_ex(
        "builtin loader gif transparent-index-oob treated opaque",
        "/tests/data/inputs/formats/gif-transparent-index-oob-static.gif",
        SIXEL_PIXELFORMAT_PAL8,
        8,
        8,
        1,
        -1,
        0,
        1,
        1,
        256,
        NULL,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_builtin_loader_hdr_case_with_cms("builtin loader hdr cms=off",
                                                  SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
                                                  SIXEL_COLORSPACE_LINEAR,
                                                  SIXEL_CMS_ENGINE_NONE);
    if (result != 0) {
        return result;
    }

    expected_cms_pixelformat_text = loader_test_getenv(
        "SIXEL_TEST_EXPECT_HDR_CMS_PIXELFORMAT");
    if (expected_cms_pixelformat_text != NULL &&
        expected_cms_pixelformat_text[0] != '\0') {
        if (parse_expected_hdr_cms_pixelformat(expected_cms_pixelformat_text,
                                               &parsed_pixelformat) != 0) {
            fprintf(stderr,
                    "builtin loader hdr cms=on: unknown expected pixelformat "
                    "'%s'\n",
                    expected_cms_pixelformat_text);
            return 1;
        }
        cms_target_pixelformat = parsed_pixelformat;
    } else {
        cms_target_pixelformat = loader_cms_target_pixelformat();
    }
    cms_target_colorspace = expected_colorspace_for_pixelformat(
        cms_target_pixelformat);
    result = run_builtin_loader_hdr_case_with_cms("builtin loader hdr cms=on",
                                                  cms_target_pixelformat,
                                                  cms_target_colorspace,
                                                  SIXEL_CMS_ENGINE_AUTO);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader gray16",
        "/tests/data/inputs/formats/"
        "snake-png-gray16.png",
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader psd rgb16 keeps float32",
        "/tests/data/inputs/formats/snake16_rgb16_raw.psd",
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader psd rgb32 keeps float32",
        "/tests/data/inputs/formats/snake16_rgb32_raw.psd",
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader psd cmyk32 keeps linear float32",
        "/tests/data/inputs/formats/snake16_cmyk32_raw.psd",
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    result = run_loader_component_case(
        "builtin loader psd lab32 keeps cielab float32",
        "/tests/data/inputs/formats/snake16_lab32_raw.psd",
        SIXEL_PIXELFORMAT_CIELABFLOAT32,
        GEOMETRY_ANY,
        GEOMETRY_ANY,
        new_builtin_component_for_pixelformat_test);
    if (result != 0) {
        return result;
    }

    psd_alpha_case.label =
        "builtin loader psd rgb8 alpha no-bgcolor keeps rgb888";
    psd_alpha_case.relative_path =
        "/tests/data/inputs/formats/snake16_rgb8_alpha.psd";
    psd_alpha_case.expect.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    psd_alpha_case.expect.width = GEOMETRY_ANY;
    psd_alpha_case.expect.height = GEOMETRY_ANY;
    psd_alpha_case.expect.callback_count = 1;
    psd_alpha_case.expect.transparent = -1;
    psd_alpha_case.expect.multiframe = 0;
    psd_alpha_case.expect.mask_present = 1;
    psd_alpha_case.expect.alpha_zero_is_transparent = 1;
    psd_alpha_case.options.require_static = 1;
    psd_alpha_case.options.use_palette = 0;
    psd_alpha_case.options.reqcolors = 256;
    psd_alpha_case.options.bgcolor = NULL;
    psd_alpha_case.new_component = new_builtin_component_for_pixelformat_test;
    result = run_loader_component_case_from_spec(&psd_alpha_case);
    if (result != 0) {
        return result;
    }

    psd_alpha_case.label =
        "builtin loader psd rgb8 alpha with-bgcolor keeps rgb888";
    psd_alpha_case.expect.mask_present = 0;
    psd_alpha_case.expect.alpha_zero_is_transparent = 0;
    psd_alpha_case.options.bgcolor = bgcolor_white;
    result = run_loader_component_case_from_spec(&psd_alpha_case);
    if (result != 0) {
        return result;
    }

    return 0;
}

int
test_loader_0014_loader_builtin_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    return run_builtin_loader_test();
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
