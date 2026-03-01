/*
 * Verify libwebp animated loader keeps frame metadata consistent and
 * propagates interruption requests from callbacks.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_WEBP
typedef struct animated_probe_context {
    int callback_count;
    int first_rgb[3];
    int second_rgb[3];
} animated_probe_context_t;

static SIXELSTATUS
capture_animated_frames(sixel_frame_t *frame, void *data)
{
    animated_probe_context_t *context;
    unsigned char const *pixels;

    context = (animated_probe_context_t *)data;
    if (sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_RGB888) {
        return SIXEL_BAD_INPUT;
    }

    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        return SIXEL_BAD_INPUT;
    }

    if (context->callback_count == 0) {
        context->first_rgb[0] = pixels[0];
        context->first_rgb[1] = pixels[1];
        context->first_rgb[2] = pixels[2];
    } else if (context->callback_count == 1) {
        context->second_rgb[0] = pixels[0];
        context->second_rgb[1] = pixels[1];
        context->second_rgb[2] = pixels[2];
    }

    context->callback_count++;

    return SIXEL_OK;
}

static SIXELSTATUS
interrupt_at_first_frame(sixel_frame_t *frame, void *data)
{
    (void)frame;
    (void)data;

    return SIXEL_INTERRUPTED;
}

static int
run_libwebp_animation_test(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    char const *source_root;
    char image_path[PATH_MAX];
    int cancel_flag;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_control;
    animated_probe_context_t context;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    cancel_flag = 0;
    fstatic = 0;
    fuse_palette = 0;
    reqcolors = 256;
    loop_control = SIXEL_LOOP_DISABLE;
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
                         WEBP_ANIMATED_IMAGE_PATH,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "libwebp animation: failed to build image path\n");
        return 1;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "libwebp animation: allocator init failed\n");
        return 1;
    }

    status = sixel_chunk_new(&chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "libwebp animation: failed to read sample\n");
        goto cleanup;
    }

    status = create_loader_component_by_name("libwebp",
                                             allocator,
                                             &component);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "libwebp animation: component init failed\n");
        goto cleanup;
    }

    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                           &fstatic);
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
    status = sixel_loader_component_setopt(component,
                                           SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                           &loop_control);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    context.callback_count = 0;
    context.first_rgb[0] = 0;
    context.first_rgb[1] = 0;
    context.first_rgb[2] = 0;
    context.second_rgb[0] = 0;
    context.second_rgb[1] = 0;
    context.second_rgb[2] = 0;

    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_animated_frames,
                                         &context);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "libwebp animation: loader returned failure (%d)\n",
                (int)status);
        goto cleanup;
    }

    if (context.callback_count != 2) {
        fprintf(stderr,
                "libwebp animation: expected 2 frames, got %d\n",
                context.callback_count);
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    if (context.first_rgb[0] <= context.first_rgb[2]) {
        fprintf(stderr,
                "libwebp animation: first frame is not red-dominant\n");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    if (context.second_rgb[2] <= context.second_rgb[0]) {
        fprintf(stderr,
                "libwebp animation: second frame is not blue-dominant\n");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    status = sixel_loader_component_load(component,
                                         chunk,
                                         interrupt_at_first_frame,
                                         NULL);
    if (status != SIXEL_INTERRUPTED) {
        fprintf(stderr,
                "libwebp animation: interruption was not propagated (%d)\n",
                (int)status);
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    result = 0;

cleanup:
    sixel_loader_component_unref(component);
    sixel_chunk_destroy(chunk);
    sixel_allocator_unref(allocator);

    return result;
}
#endif

int
test_loader_0018_loader_libwebp_animation_frames(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_WEBP
    return run_libwebp_animation_test();
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
