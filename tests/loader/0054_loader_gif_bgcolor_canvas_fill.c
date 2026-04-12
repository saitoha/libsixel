/*
 * Verify GIF logical-screen background fill follows the configured
 * background-selection policy when compositing starts from a partially
 * covered first frame.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sixel.h>

#include "src/fromgif.h"
#include "src/loader-common.h"

typedef struct gif_canvas_probe {
    int calls;
    int width;
    int height;
    int pixelformat;
    size_t pixel_bytes;
    unsigned char pixels[12];
} gif_canvas_probe_t;

typedef union gif_canvas_probe_callback {
    sixel_load_image_function fn;
    void *p;
} gif_canvas_probe_callback_t;

static SIXELSTATUS
gif_canvas_probe_capture(sixel_frame_t *frame, void *context)
{
    gif_canvas_probe_t *probe;
    unsigned char const *pixels;
    size_t bytes;
    int width;
    int height;
    int pixelformat;

    probe = NULL;
    pixels = NULL;
    bytes = 0u;
    width = 0;
    height = 0;
    pixelformat = 0;
    if (frame == NULL || context == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    probe = (gif_canvas_probe_t *)context;
    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    if (width <= 0 || height <= 0) {
        return SIXEL_BAD_INPUT;
    }
    if (pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        return SIXEL_BAD_INPUT;
    }

    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    bytes = (size_t)width * (size_t)height * 3u;
    if (bytes > sizeof(probe->pixels)) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    pixels = sixel_frame_get_pixels(frame);
    if (pixels == NULL) {
        return SIXEL_BAD_INPUT;
    }

    probe->width = width;
    probe->height = height;
    probe->pixelformat = pixelformat;
    probe->pixel_bytes = bytes;
    memcpy(probe->pixels, pixels, bytes);
    probe->calls += 1;

    return SIXEL_OK;
}

static int
run_case(char const *label,
         unsigned char const bgcolor[3],
         unsigned char const expected[12])
{
    static unsigned char const sample_gif[] = {
        0x47u, 0x49u, 0x46u, 0x38u, 0x39u, 0x61u,
        0x02u, 0x00u, 0x02u, 0x00u, 0x80u, 0x00u, 0x00u,
        0xffu, 0xffu, 0xffu, 0x00u, 0x00u, 0x00u,
        0x21u, 0xf9u, 0x04u, 0x01u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x2cu, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x01u, 0x00u,
        0x00u, 0x02u, 0x02u, 0x4cu, 0x01u, 0x00u, 0x3bu
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    gif_canvas_probe_t probe;
    gif_canvas_probe_callback_t callback;
    unsigned char data[sizeof(sample_gif)];

    status = SIXEL_FALSE;
    allocator = NULL;
    memset(&probe, 0, sizeof(probe));
    callback.p = NULL;
    memset(data, 0, sizeof(data));
    memcpy(data, sample_gif, sizeof(sample_gif));
    callback.fn = gif_canvas_probe_capture;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: allocator init failed (%d)\n", label, status);
        return 1;
    }

    status = load_gif(data,
                      (int)sizeof(data),
                      (unsigned char *)bgcolor,
                      SIXEL_LOADER_BGCOLOR_SOURCE_EXPLICIT,
                      256,
                      0,
                      1,
                      SIXEL_LOOP_DISABLE,
                      INT_MIN,
                      callback.p,
                      &probe,
                      NULL,
                      allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: load_gif failed (%d)\n", label, status);
        sixel_allocator_unref(allocator);
        return 1;
    }
    if (probe.calls != 1 || probe.width != 2 || probe.height != 2 ||
            probe.pixel_bytes != 12u ||
            probe.pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        fprintf(stderr,
                "%s: unexpected callback metadata calls=%d size=%dx%d "
                "bytes=%lu fmt=%d\n",
                label,
                probe.calls,
                probe.width,
                probe.height,
                (unsigned long)probe.pixel_bytes,
                probe.pixelformat);
        sixel_allocator_unref(allocator);
        return 1;
    }
    if (memcmp(probe.pixels, expected, 12u) != 0) {
        fprintf(stderr, "%s: canvas fill did not match expected RGB\n", label);
        sixel_allocator_unref(allocator);
        return 1;
    }

    sixel_allocator_unref(allocator);
    return 0;
}

int
test_loader_0054_loader_gif_bgcolor_canvas_fill(int argc, char **argv)
{
    static unsigned char const black[3] = { 0x00u, 0x00u, 0x00u };
    static unsigned char const white[3] = { 0xffu, 0xffu, 0xffu };
    static unsigned char const expect_black[12] = {
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u
    };
    static unsigned char const expect_white[12] = {
        0x00u, 0x00u, 0x00u,
        0xffu, 0xffu, 0xffu,
        0xffu, 0xffu, 0xffu,
        0xffu, 0xffu, 0xffu
    };
    unsigned char const *expected_black_case;
    int background_policy;

    (void)argc;
    (void)argv;
    expected_black_case = NULL;
    background_policy = SIXEL_LOADER_BACKGROUND_POLICY_FILE_FIRST;

    background_policy = loader_background_policy();
    if (background_policy == SIXEL_LOADER_BACKGROUND_POLICY_EXPLICIT_FIRST) {
        expected_black_case = expect_black;
    } else {
        expected_black_case = expect_white;
    }

    if (run_case("gif bgcolor black", black, expected_black_case) != 0) {
        return EXIT_FAILURE;
    }
    if (run_case("gif bgcolor white", white, expect_white) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
