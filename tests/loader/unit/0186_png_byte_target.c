/* SPDX-License-Identifier: MIT */
/* Preserve the last byte rounding at the PNG caller's explicit target. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/frame-private.h"
#include "src/loader.h"
#include "src/loader-manager.h"
#include "tests/data/color-numeric/png-byte-target.h"

/* These specimens preserve the source-to-target rounding contract. The
 * builtin loader now selects a shared source profile in either build. */
typedef struct {
    int index;
    int stage;
    int calls;
} png_byte_test_t;

static SIXELSTATUS
png_byte_capture(sixel_frame_t *frame, void *context)
{
    png_byte_test_t *probe;
    unsigned char const *pixels;
    uint32_t actual;
    uint32_t expected;
    int format;
    int space;
    int c;

    probe = (png_byte_test_t *)context;
    ++probe->calls;
    format = probe->stage == 3 ? SIXEL_PIXELFORMAT_RGB888 :
        probe->stage == 1 ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
        SIXEL_PIXELFORMAT_RGBFLOAT32;
    space = probe->stage == 1 ? SIXEL_COLORSPACE_LINEAR :
        SIXEL_COLORSPACE_GAMMA;
    if (frame->width != 1 || frame->height != 1 ||
        frame->pixelformat != format || frame->colorspace != space ||
        frame->transparent_mask_size != 0u ||
        frame->alpha_zero_is_transparent != 0) {
        fprintf(stderr, "PNG byte target stage %d frame contract\n",
            probe->stage);
        return SIXEL_BAD_INPUT;
    }
    expected = probe->stage == 0 ? color_png_byte_target[probe->index].source :
        probe->stage == 1 ? color_png_byte_target[probe->index].linear :
        probe->stage == 2 ? color_png_byte_target[probe->index].gamma :
        color_png_byte_target[probe->index].byte;
    pixels = sixel_frame_get_pixels(frame);
    for (c = 0; c < 3; ++c) {
        if (probe->stage == 3) {
            actual = pixels[c];
        } else {
            /* Compare exact IEEE binary32 storage from the independent
             * source integer/gAMA oracle, not a production roundtrip. */
            memcpy(&actual, pixels + c * sizeof(float), sizeof(actual));
        }
        if (actual != expected) {
            fprintf(stderr, "PNG byte target stage %d channel %d: %u != %u\n",
                probe->stage, c, (unsigned int)actual,
                (unsigned int)expected);
            return SIXEL_BAD_INPUT;
        }
    }
    return SIXEL_OK;
}

int
test_loader_0186_png_byte_target(int argc, char **argv)
{
    png_byte_test_t probe;
    sixel_cms_engine_t previous;
    sixel_loader_t *loader;
    SIXELSTATUS status;
    char order[160];
    int no;
    int length;

    if (argc != 4 || (strcmp(argv[2], "0") != 0 &&
                      strcmp(argv[2], "1") != 0)) {
        return 1;
    }
#if HAVE_LCMS2
    /* The libpng calibrated-profile path has separate precision coverage. */
    if (strcmp(argv[1], "builtin") != 0) { return 77; }
#endif
    if (!loader_manager_entry_available(argv[1])) {
        return 77;
    }
    memset(&probe, 0, sizeof(probe));
    probe.index = atoi(argv[2]);
    previous = sixel_cms_get_engine();
    loader = NULL;
    no = 0;
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_CONTEXT, &probe);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_USE_PALETTE, &no);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    for (probe.stage = 0; probe.stage < 4; ++probe.stage) {
        /* CMS none keeps original 16-bit gamma samples. The other loads
         * distinguish linear storage, a requested gamma float view, and
         * direct byte conversion without an unwanted float gamma store. */
        length = snprintf(order, sizeof(order),
            "%s:cms_engine=%s:cms_target=%s:prefer_8bit=%d!", argv[1],
            probe.stage == 0 ? "none" : "builtin",
            probe.stage == 1 ? "linear" : "gamma",
            probe.stage == 0 || probe.stage == 3);
        if (length < 0 || (size_t)length >= sizeof(order)) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        status = sixel_loader_setopt(loader, SIXEL_LOADER_OPTION_LOADER_ORDER,
            order);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_loader_load_file(loader, argv[3], png_byte_capture);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }
end:
    sixel_loader_unref(loader);
    sixel_cms_set_engine(previous);
    return SIXEL_FAILED(status) || probe.calls != 4;
}
