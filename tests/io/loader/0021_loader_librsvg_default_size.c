/*
 * Verify librsvg loader falls back to the default 300x150 viewport.
 */

#include <stdio.h>
#include <string.h>

#include "tests/io/loader/librsvg_test_common.h"

int
test_loader_0021_loader_librsvg_default_size(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#if HAVE_LIBRSVG
    unsigned char const svg[] =
        "<svg xmlns='http://www.w3.org/2000/svg'>"
        "<rect x='0' y='0' width='10' height='10' fill='#ff0000'/></svg>";
    librsvg_probe_context_t probe;

    memset(&probe, 0, sizeof(probe));
    if (run_librsvg_component_chunk_case(svg,
                                         sizeof(svg) - 1,
                                         NULL,
                                         &probe) != 0) {
        fprintf(stderr, "librsvg load failed\n");
        return 1;
    }
    if (probe.callback_count != 1 ||
        probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr, "unexpected callback or pixelformat\n");
        return 1;
    }
    if (probe.width != 300 || probe.height != 150) {
        fprintf(stderr,
                "unexpected default geometry %dx%d\n",
                probe.width,
                probe.height);
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
