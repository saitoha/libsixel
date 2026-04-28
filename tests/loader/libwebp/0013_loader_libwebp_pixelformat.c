/*
 * Verify libwebp loader reports float32 RGB output by default and PAL8 when
 * palette promotion is requested for indexed sources.
 */

#include "tests/loader/pixelformat_test_common.h"
#include "src/loader-common.h"

#if HAVE_WEBP
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
                         path,
                         image_path,
                         sizeof(image_path)) != 0) {
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }

    status = sixel_chunk_new(&chunk, image_path, 0, &cancel_flag, allocator);
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
    sixel_chunk_destroy(chunk);
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
                                                  SIXEL_PIXELFORMAT_RGBFLOAT32,
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
                                                  SIXEL_PIXELFORMAT_RGBFLOAT32,
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
        SIXEL_PIXELFORMAT_RGBFLOAT32,
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
