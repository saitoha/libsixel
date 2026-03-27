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
 * - HDR(RGBE) -> LINEARRGBFLOAT32
 * - Gray(16-bit) -> RGBFLOAT32 (no 8-bit precision loss)
 */

#include <math.h>
#include <string.h>

#include "tests/loader/pixelformat_test_common.h"
#include "src/cms.h"
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
run_builtin_loader_hdr_numeric_probe_case(char const *label,
                                          char const *relative_path,
                                          int cms_engine,
                                          hdr_numeric_probe_context_t *context)
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

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    cancel_flag = 0;
    require_static = 1;
    use_palette = 0;
    reqcolors = 256;

    if (context == NULL) {
        fprintf(stderr, "%s: context is null\n", label);
        return 1;
    }

    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

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

    memset(context, 0, sizeof(*context));
    callback_state.loader = NULL;
    callback_state.fn = capture_hdr_numeric_probe;
    callback_state.context = context;

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

    if (context->callback_count != 1) {
        fprintf(stderr, "%s: callback count mismatch\n", label);
        goto cleanup;
    }
    if (context->width <= 0 || context->height <= 0) {
        fprintf(stderr,
                "%s: invalid geometry %dx%d\n",
                label,
                context->width,
                context->height);
        goto cleanup;
    }

    status = SIXEL_OK;

cleanup:
    sixel_loader_component_unref(component);
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status) ? 0 : 1;
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

    source_root = getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

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
run_builtin_loader_test(void)
{
    char const *hdr_numeric_mode;
    char const *expected_cms_pixelformat_text;
    unsigned char const bgcolor_white[3] = { 0xffu, 0xffu, 0xffu };
    int cms_target_pixelformat;
    int cms_target_colorspace;
    int parsed_pixelformat;
    int result;

    hdr_numeric_mode = getenv("SIXEL_TEST_HDR_NUMERIC_GAMMA");
    if (hdr_numeric_mode != NULL && strcmp(hdr_numeric_mode, "1") == 0) {
        return run_builtin_loader_hdr_gamma_numeric_test();
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

    expected_cms_pixelformat_text = getenv(
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

    return run_loader_component_case("builtin loader gray16",
                                     "/tests/data/inputs/formats/snake-png-gray16.png",
                                     SIXEL_PIXELFORMAT_RGBFLOAT32,
                                     GEOMETRY_ANY,
                                     GEOMETRY_ANY,
                                     new_builtin_component_for_pixelformat_test);
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
