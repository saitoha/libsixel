/*
 * Verify libpng loader falls back from indexed PNG when reqcolors is lower
 * than the source palette size.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_LIBPNG
static SIXELSTATUS
new_libpng_component(sixel_allocator_t *allocator,
                     sixel_loader_component_t **ppcomponent)
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
    return run_loader_component_case_with_options(
        "libpng loader indexed png reqcolors fallback",
        "/tests/data/inputs/formats/snake-png-pal8.png",
        SIXEL_PIXELFORMAT_RGB888,
        64,
        64,
        1,
        1,
        253,
        new_libpng_component);
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
