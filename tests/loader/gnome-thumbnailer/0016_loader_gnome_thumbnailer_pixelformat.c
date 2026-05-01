/*
 * Verify GNOME thumbnailer path reports linear-float output for RGBA sources.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_FREEDESKTOP_THUMBNAILING
static SIXELSTATUS
new_gnome_thumbnailer_component(sixel_allocator_t *allocator,
                                void **ppcomponent)
{
    return create_loader_component_by_name("gnome-thumbnailer",
                                           allocator,
                                           ppcomponent);
}

static int
run_thumbnailer_loader_test(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_chunk_t *chunk;
    sixel_loader_component_t *component;
    loader_probe_context_t context;
    char const *source_root;
    char const *message;
    char image_path[PATH_MAX];
    int cancel_flag;
    int require_static;
    int use_palette;
    int reqcolors;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    chunk = NULL;
    component = NULL;
    source_root = NULL;
    message = NULL;
    image_path[0] = '\0';
    cancel_flag = 0;
    require_static = 1;
    use_palette = 0;
    reqcolors = SIXEL_PALETTE_MAX;
    result = 1;
    memset(&context, 0, sizeof(context));

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
                         RGBA_IMAGE_PATH,
                         image_path,
                         sizeof(image_path)) != 0) {
        fprintf(stderr, "GNOME thumbnailer: failed to build image path\n");
        return 1;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "GNOME thumbnailer: allocator init failed\n");
        return 1;
    }
    status = sixel_chunk_create_from_source(&chunk,
                             image_path,
                             0,
                             &cancel_flag,
                             allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "GNOME thumbnailer: failed to read sample\n");
        goto cleanup;
    }
    status = new_gnome_thumbnailer_component(allocator, (void **)&component);
    if (SIXEL_FAILED(status) || component == NULL) {
        fprintf(stderr, "GNOME thumbnailer unavailable\n");
        result = SIXEL_TEST_SKIP;
        goto cleanup;
    }

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
                                           SIXEL_LOADER_OPTION_BGCOLOR,
                                           NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    sixel_helper_set_additional_message(NULL);
    status = sixel_loader_component_load(component,
                                         chunk,
                                         capture_frame,
                                         &context);
    if (SIXEL_FAILED(status)) {
        message = sixel_helper_get_additional_message();
        if (message != NULL &&
            strstr(message, "no thumbnailer available") != NULL) {
            fprintf(stderr,
                    "GNOME thumbnailer unavailable: "
                    "no thumbnailer command is runnable\n");
            result = SIXEL_TEST_SKIP;
            goto cleanup;
        }
        fprintf(stderr,
                "GNOME thumbnailer: loader reported failure (%d)\n",
                (int)status);
        goto cleanup;
    }
    if (context.callback_count <= 0 ||
        context.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
        fprintf(stderr,
                "GNOME thumbnailer: pixelformat mismatch (%d)\n",
                context.pixelformat);
        goto cleanup;
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
#endif

int
test_loader_0016_loader_gnome_thumbnailer_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_FREEDESKTOP_THUMBNAILING
    return run_thumbnailer_loader_test();
#else
    fprintf(stderr, "GNOME thumbnailer unavailable\n");
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
