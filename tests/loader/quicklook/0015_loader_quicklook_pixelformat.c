/*
 * Verify QuickLook loader reports RGBA output for RGBA sources.
 */

#include "tests/loader/pixelformat_test_common.h"

#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
static SIXELSTATUS
new_quicklook_component(sixel_allocator_t *allocator,
                        sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("quicklook", allocator, ppcomponent);
}

static int
run_quicklook_loader_test(void)
{
    return run_loader_component_case("QuickLook loader",
                                     RGBA_IMAGE_PATH,
                                     SIXEL_PIXELFORMAT_RGBA8888,
                                     GEOMETRY_ANY,
                                     GEOMETRY_ANY,
                                     new_quicklook_component);
}
#endif

int
test_loader_0015_loader_quicklook_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    return run_quicklook_loader_test();
#else
    fprintf(stderr, "QuickLook loader unavailable\n");
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
