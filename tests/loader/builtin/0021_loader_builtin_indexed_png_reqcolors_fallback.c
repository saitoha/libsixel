/*
 * Verify builtin loader falls back from indexed PNG when reqcolors is lower
 * than the source palette size.
 */

#include "tests/loader/pixelformat_test_common.h"

static SIXELSTATUS
new_builtin_component(sixel_allocator_t *allocator,
                      sixel_loader_component_t **ppcomponent)
{
    return create_loader_component_by_name("builtin", allocator, ppcomponent);
}

int
test_loader_0021_loader_builtin_indexed_png_reqcolors_fallback(int argc,
                                                                char **argv)
{
    (void)argc;
    (void)argv;

    return run_loader_component_case_with_options(
        "builtin loader indexed png reqcolors fallback",
        "/tests/data/inputs/formats/snake-png-pal8.png",
        SIXEL_PIXELFORMAT_RGB888,
        64,
        64,
        1,
        1,
        253,
        new_builtin_component);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
