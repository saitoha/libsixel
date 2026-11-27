/*
 * Verify libwebp loader reports byte RGB output by default and PAL8 when
 * palette promotion is requested for indexed sources.
 */

#include <math.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "src/loader-common.h"
#include "src/pixelformat.h"

#if HAVE_WEBP
static double
webp_test_srgb(unsigned char sample)
{
    double value;

    value = sample / 255.0;
    return value <= 0.04045 ? value / 12.92
                           : pow((value + 0.055) / 1.055, 2.4);
}

static SIXELSTATUS
webp_test_capture_linear_alpha(sixel_frame_t *frame, void *context)
{
    static unsigned char const rgb[] = {128, 64, 192, 32, 200, 96};
    static unsigned char const bg[] = {32, 96, 160};
    sixel_frame_pixels_view_t view;
    unsigned int *calls;
    double expected;
    double alpha;
    size_t i;

    calls = context;
    ++*calls;
    if (SIXEL_FAILED(loader_test_frame_get_pixels_view(frame, &view)) ||
        view.width != 2 || view.height != 1 ||
        view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        view.colorspace != SIXEL_COLORSPACE_SRGB_LINEAR ||
        view.rgb_definition != NULL || view.pixels_float32 == NULL) {
        fprintf(stderr, "WebP alpha output must be linear sRGB float32\n");
        return SIXEL_BAD_INPUT;
    }
    for (i = 0u; i < sizeof(rgb); ++i) {
        alpha = i < 3u ? 128.0 / 255.0 : 1.0;
        expected = alpha * webp_test_srgb(rgb[i]) +
                   (1.0 - alpha) * webp_test_srgb(bg[i % 3u]);
        if (!(fabs(view.pixels_float32[i] - expected) < 2e-7)) {
            fprintf(stderr, "WebP sample %zu: %.9g vs %.9g\n",
                    i, view.pixels_float32[i], expected);
            return SIXEL_BAD_INPUT;
        }
    }
    return SIXEL_OK;
}

static int
webp_test_linear_alpha(void)
{
    /* Lossless input encoded with cwebp -lossless -exact -z 9 from
     * RGBA pixels (128,64,192,128) and (32,200,96,255). */
    static unsigned char const webp[] = {
        0x52, 0x49, 0x46, 0x46, 0x1e, 0x00, 0x00, 0x00, 0x57, 0x45, 0x42, 0x50,
        0x56, 0x50, 0x38, 0x4c, 0x11, 0x00, 0x00, 0x00, 0x2f, 0x01, 0x00, 0x00,
        0x10, 0x38, 0x10, 0xf2, 0x41, 0x00, 0x0f, 0x06, 0x7c, 0xc0, 0xff, 0xb0,
        0x04, 0x00
    };
    unsigned char bg[] = {32, 96, 160};
    sixel_allocator_t *allocator = NULL;
    sixel_loader_component_t *component = NULL;
    sixel_chunk_t *chunk = NULL;
    unsigned int calls = 0u;
    loader_probe_callback_state_t callback = {
        NULL, webp_test_capture_linear_alpha, &calls
    };
    int no = 0;
    int ok;

    sixel_helper_set_loader_background_colorspace(SIXEL_COLORSPACE_SRGB);
    ok = SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
                                               NULL)) &&
        SIXEL_SUCCEEDED(create_loader_component_by_name(
            "libwebp", allocator, (void **)&component)) &&
        SIXEL_SUCCEEDED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_COMPONENT_OPTION_LIBWEBP_ENABLE_CMS, &no)) &&
        SIXEL_SUCCEEDED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_USE_PALETTE, &no)) &&
        SIXEL_SUCCEEDED(sixel_loader_component_setopt(component,
            SIXEL_LOADER_OPTION_BGCOLOR, bg)) &&
        SIXEL_SUCCEEDED(sixel_chunk_create_from_memory(&chunk, webp,
            sizeof(webp), NULL, allocator)) &&
        SIXEL_SUCCEEDED(sixel_loader_component_load(component, chunk,
            capture_frame_trampoline, &callback)) && calls == 1u;
    if (!ok) {
        fprintf(stderr, "%s\n", sixel_helper_get_additional_message());
    }
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_loader_component_unref(component);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
run_libwebp_loader_case_with_options(char const *label,
                                     char const *path,
                                     int expected_pixelformat,
                                     int expected_width,
                                     int expected_height,
                                     int enable_cms,
                                     int fstatic,
                                     int fuse_palette,
                                     int reqcolors)
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

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    source_root = NULL;
    cancel_flag = 0;
    result = 1;

    source_root = sixel_compat_getenv("MESON_SOURCE_ROOT");
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("abs_top_srcdir");
    }
    if (source_root == NULL) {
        source_root = sixel_compat_getenv("TOP_SRCDIR");
    }
    if (source_root == NULL) {
        source_root = ".";
    }

    if (build_image_path(source_root,
                         path,
                         image_path,
                         sizeof(image_path)) != 0) {
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }

    status = sixel_chunk_create_from_source(&chunk, image_path, 0, &cancel_flag, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = create_loader_component_by_name("libwebp",
                                             allocator,
                                             (void **)&component);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    context.callback_count = 0;
    context.pixelformat = 0;
    context.width = 0;
    context.height = 0;
    callback_state.loader = NULL;
    callback_state.fn = capture_frame;
    callback_state.context = &context;

    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &fstatic);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(
        component,
        SIXEL_LOADER_COMPONENT_OPTION_LIBWEBP_ENABLE_CMS,
        &enable_cms);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_USE_PALETTE,
                                           &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQCOLORS,
                                           &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame_trampoline,
                                         &callback_state);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (context.callback_count != 1 ||
        context.pixelformat != expected_pixelformat ||
        context.width != expected_width ||
        context.height != expected_height) {
        fprintf(stderr,
                "%s expected pixelformat=%d size=%dx%d got pixelformat=%d size=%dx%d callbacks=%d\n",
                label,
                expected_pixelformat,
                expected_width,
                expected_height,
                context.pixelformat,
                context.width,
                context.height,
                context.callback_count);
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_allocator_unref(allocator);
    if (result != 0) {
        fprintf(stderr, "%s failed\n", label);
    }
    return result;
}

static int
run_libwebp_loader_test(void)
{
    int status;
    int cms_target_pixelformat;

    cms_target_pixelformat = loader_cms_target_pixelformat();

    status = run_libwebp_loader_case_with_options("libwebp loader cms=0",
                                                  WEBP_IMAGE_PATH,
                                                  SIXEL_PIXELFORMAT_RGB888,
                                                  64,
                                                  64,
                                                  0,
                                                  1,
                                                  0,
                                                  256);
    if (status != 0) {
        return status;
    }

    status = run_libwebp_loader_case_with_options("libwebp loader cms=1",
                                                  WEBP_IMAGE_PATH,
                                                  SIXEL_PIXELFORMAT_RGB888,
                                                  64,
                                                  64,
                                                  1,
                                                  1,
                                                  0,
                                                  256);
    if (status != 0) {
        return status;
    }

    status = run_libwebp_loader_case_with_options(
        "libwebp embedded ICC cms=0",
        "/tests/data/inputs/snake_64_embedded_a98_icc.webp",
        SIXEL_PIXELFORMAT_RGB888,
        64,
        64,
        0,
        1,
        0,
        256);
    if (status != 0) {
        return status;
    }

    status = run_libwebp_loader_case_with_options(
        "libwebp embedded ICC cms=1",
        "/tests/data/inputs/snake_64_embedded_a98_icc.webp",
        cms_target_pixelformat,
        64,
        64,
        1,
        1,
        0,
        256);
    if (status != 0) {
        return status;
    }

    return run_libwebp_loader_case_with_options(
        "libwebp loader palette mode",
        "/tests/data/inputs/formats/animated-lossless-8x8-2frame-min.webp",
        SIXEL_PIXELFORMAT_PAL8,
        8,
        8,
        1,
        1,
        1,
        8);
}
#endif

int
test_loader_0017_loader_libwebp_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_WEBP
    if (argc == 2 && strcmp(argv[1], "linear-alpha") == 0) {
        return webp_test_linear_alpha();
    }
    return run_libwebp_loader_test();
#else
    fprintf(stderr, "libwebp loader unavailable\n");
    return SIXEL_TEST_SKIP;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
