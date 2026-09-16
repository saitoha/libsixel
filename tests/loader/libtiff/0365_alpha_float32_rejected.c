/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include "src/loader.h"

#if HAVE_LIBTIFF
static int ar_calls;

static SIXELSTATUS
ar_capture(sixel_frame_t *frame, void *context)
{
    (void)frame;
    (void)context;
    ++ar_calls;
    return SIXEL_OK;
}
#endif

int
test_loader_0365_alpha16(int argc, char **argv)
{
#if HAVE_LIBTIFF
    char const *photos[] = { "rgb", "black", "white" };
    sixel_loader_t *loader;
    SIXELSTATUS status;
    char path[4096];
    int photo;
    int association;
    int ok;

    if (argc != 2) { return 2; }
    ok = 1;
    for (photo = 0; ok && photo < 3; ++photo) {
        for (association = 1; ok && association <= 2; ++association) {
            loader = NULL;
            ar_calls = 0;
            snprintf(path, sizeof(path), "%s/tiff-alpha-%s-32-a%d.tif",
                     argv[1], photos[photo], association);
            status = sixel_loader_new(&loader, NULL);
            if (SIXEL_SUCCEEDED(status)) {
                status = sixel_loader_setopt(loader,
                    SIXEL_LOADER_OPTION_LOADER_ORDER,
                    "libtiff:cms_engine=builtin:cms_target=linear!");
            }
            if (SIXEL_FAILED(status)) {
                sixel_loader_unref(loader);
                return 1;
            }
            status = sixel_loader_load_file(loader, path, ar_capture);
            ok = status == SIXEL_TIFF_ERROR && ar_calls == 0;
            sixel_loader_unref(loader);
        }
    }
    return ok ? 0 : 1;
#else
    (void)argc; (void)argv;
    return 77;
#endif
}
