/*
 * Verify librsvg loader applies SIXEL_LOADER_OPTION_BGCOLOR to transparent SVG.
 */

#include <stdio.h>
#include <string.h>

#include "tests/io/loader/librsvg_test_common.h"

int
test_loader_0023_loader_librsvg_bgcolor_option(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_LIBRSVG
    unsigned char const svg[] =
        "<svg xmlns='http://www.w3.org/2000/svg' width='2' height='1'></svg>";
    unsigned char const bgcolor[3] = { 0x12, 0x34, 0x56 };
    librsvg_probe_context_t probe;

    memset(&probe, 0, sizeof(probe));
    if (run_librsvg_component_chunk_case(svg,
                                         sizeof(svg) - 1,
                                         bgcolor,
                                         &probe) != 0) {
        fprintf(stderr, "librsvg load failed\n");
        return 1;
    }
    if (probe.width != 2 || probe.height != 1) {
        fprintf(stderr,
                "unexpected geometry %dx%d\n",
                probe.width,
                probe.height);
        return 1;
    }
    if (probe.first_pixel[0] != bgcolor[0] ||
        probe.first_pixel[1] != bgcolor[1] ||
        probe.first_pixel[2] != bgcolor[2]) {
        fprintf(stderr,
                "unexpected first pixel %02x %02x %02x\n",
                probe.first_pixel[0],
                probe.first_pixel[1],
                probe.first_pixel[2]);
        return 1;
    }

    return 0;
#else
    fprintf(stderr, "librsvg loader unavailable\n");
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
