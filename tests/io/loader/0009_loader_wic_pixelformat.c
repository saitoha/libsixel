/*
 * Verify WIC loader reports RGBA output for RGBA sources.
 */

#include "tests/io/loader/pixelformat_test_common.h"

#if HAVE_WIC
static SIXELSTATUS
new_wic_component(sixel_allocator_t *allocator,
                  sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("wic", allocator, ppcomponent);
}

static int
run_wic_loader_test(void)
{
    return run_loader_component_case("WIC loader",
                                     RGBA_IMAGE_PATH,
                                     SIXEL_PIXELFORMAT_RGBA8888,
                                     2,
                                     1,
                                     new_wic_component);
}
#endif

int
test_loader_0009_loader_wic_pixelformat(int argc, char **argv)
{
    (void) argc;
    (void) argv;

#if HAVE_WIC
    return run_wic_loader_test();
#else
    fprintf(stderr, "WIC loader unavailable\n");
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
