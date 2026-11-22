/*
 * Verify libpng loader falls back from indexed PNG when reqcolors is lower
 * than the source palette size.
 */

#include "tests/_support/loader/pixelformat_test_common.h"

#if HAVE_LIBPNG
/* Match the actual two-byte pixel allocation to the loader's output view.
 * A logical size calculation alone also passes for an oversized buffer. */
static void *packed_pixels;
static void *packed_palette;

static void *
record_packed_malloc(size_t size)
{
    void *p = malloc(size);

    if (size == 2) {
        packed_pixels = p;
    } else if (size == 6) {
        packed_palette = p;
    }
    return p;
}

static SIXELSTATUS
check_packed_png(sixel_frame_t *frame, void *context)
{
    unsigned char const palette[] = {0, 0, 0, 255, 0, 0};
    sixel_frame_interface_t *iface = sixel_frame_as_interface(frame);
    sixel_frame_pixels_view_t view;
    int *mode = context;
    int i;
    size_t size;

    if (SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
        view.width != 16 || view.height != 1 || view.pixels == NULL ||
        view.ncolors != 2 || view.palette == NULL ||
        memcmp(view.palette, palette, sizeof(palette)) != 0) {
        return SIXEL_BAD_INPUT;
    }
    if (*mode == 0) {
        if (view.pixelformat != SIXEL_PIXELFORMAT_PAL1 ||
            view.pixels != packed_pixels || view.palette != packed_palette ||
            view.pixels[0] != 0 || view.pixels[1] != 1 ||
            SIXEL_FAILED(iface->vtbl->measure_storage(iface, &size)) ||
            size != 2 + sizeof(palette)) {
            return SIXEL_BAD_INPUT;
        }
    } else {
        if (view.pixelformat != SIXEL_PIXELFORMAT_PAL8) {
            return SIXEL_BAD_INPUT;
        }
        for (i = 0; i < 16; ++i) {
            if (view.pixels[i] != (i == 0)) {
                return SIXEL_BAD_INPUT;
            }
        }
    }
    *mode = 2;
    return SIXEL_OK;
}

static int
test_packed_png(int mirror)
{
    /* PAL1, black with one red pixel at the right edge, EXIF orientation 2. */
    static unsigned char const wire[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00,
        0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x10,
        0x00, 0x00, 0x00, 0x01, 0x01, 0x03, 0x00, 0x00, 0x00, 0xed,
        0xb7, 0xbc, 0x1c, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4c, 0x54,
        0x45, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x1b, 0xff, 0x8d,
        0x22, 0x00, 0x00, 0x00, 0x1a, 0x65, 0x58, 0x49, 0x66, 0x49,
        0x49, 0x2a, 0x00, 0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x12,
        0x01, 0x03, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x02, 0x1f, 0xd3, 0x00,
        0x00, 0x00, 0x0b, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63,
        0x60, 0x60, 0x04, 0x00, 0x00, 0x04, 0x00, 0x02, 0xbf, 0x7a,
        0x3f, 0x4a, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44,
        0xae, 0x42, 0x60, 0x82,
    };
    sixel_loader_t *loader = NULL;
    sixel_chunk_t *chunk = NULL;
    sixel_allocator_t *allocator = NULL;
    int mode = mirror;
    int result = EXIT_FAILURE;

    packed_pixels = packed_palette = NULL;
    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, record_packed_malloc,
            calloc, realloc, free)) &&
        SIXEL_SUCCEEDED(sixel_loader_new(&loader, allocator)) &&
        SIXEL_SUCCEEDED(sixel_chunk_create_from_memory(&chunk, wire,
            sizeof(wire), NULL, allocator)) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_LOADER_ORDER,
            mirror ? "libpng:orientation=1!" : "libpng:orientation=0!")) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_CONTEXT, &mode)) &&
        SIXEL_SUCCEEDED(sixel_loader_load_chunk(loader, chunk,
            check_packed_png)) && mode == 2) {
        result = EXIT_SUCCESS;
    }
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_loader_unref(loader);
    sixel_allocator_unref(allocator);
    return result;
}

static SIXELSTATUS
new_libpng_component_0022(sixel_allocator_t *allocator,
                          void **ppcomponent)
{
    return create_loader_component_by_name("libpng", allocator, ppcomponent);
}
#endif

int
test_loader_0022_loader_libpng_indexed_png_reqcolors_fallback(int argc,
                                                               char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_LIBPNG
    if (argc == 2 && strcmp(argv[1], "packed-storage") == 0) {
        return test_packed_png(0);
    }
    if (argc == 2 && strcmp(argv[1], "mirror") == 0) {
        return test_packed_png(1);
    }
    if (argc != 1) {
        return EXIT_FAILURE;
    }
    return run_loader_component_case_with_options(
        "libpng loader indexed png reqcolors fallback",
        "/tests/data/inputs/formats/snake-png-pal8.png",
        SIXEL_PIXELFORMAT_RGB888,
        64,
        64,
        1,
        1,
        253,
        new_libpng_component_0022);
#else
    fprintf(stderr, "libpng loader unavailable\n");
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
