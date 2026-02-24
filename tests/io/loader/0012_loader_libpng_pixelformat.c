/*
 * Verify libpng loader reports RGB output for RGBA sources.
 */

#include "tests/io/loader/pixelformat_test_common.h"

#if HAVE_LIBPNG
static SIXELSTATUS
new_libpng_component(sixel_allocator_t *allocator,
                     sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("libpng", allocator, ppcomponent);
}

static int
run_libpng_loader_test(void)
{
    return run_loader_component_case("libpng loader",
                                     RGBA_IMAGE_PATH,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     2,
                                     1,
                                     new_libpng_component);
}
#endif

int
test_loader_0012_loader_libpng_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_LIBPNG
    return run_libpng_loader_test();
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
