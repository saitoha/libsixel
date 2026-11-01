/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that legal large byte inputs retain the linear float32 resize path.
 * The source dimensions require more than 64 MiB as RGB float32 but remain
 * below the documented per-allocation limit.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/encoder.h"
#include "src/frame.h"
#include "src/planner.h"

#define PRLF_WIDTH 4096
#define PRLF_HEIGHT 1366

static int
planner_linear_float32_valid(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_encoding_planner_t planner;
    unsigned char pixels[3];

    status = SIXEL_FALSE;
    allocator = NULL;
    encoder = NULL;
    frame = NULL;
    pixels[0] = 0u;
    pixels[1] = 0u;
    pixels[2] = 0u;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame,
                                       pixels,
                                       PRLF_WIDTH,
                                       PRLF_HEIGHT,
                                       SIXEL_PIXELFORMAT_RGB888,
                                       NULL,
                                       -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    encoder->working_colorspace = SIXEL_COLORSPACE_OKLAB;
    encoder->working_colorspace_set = 1;
    encoder->force_float32_colorspace = 1;
    encoder->prefer_float32 = 1;
    encoder->percentwidth = 50;
    sixel_encoding_planner_init(&planner);
    sixel_encoding_planner_analyze(&planner, encoder, frame);

    if (planner.scale_active == 0 ||
            planner.scale_input_pixelformat !=
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
            planner.scale_pixelformat !=
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
            planner.colorspace_before_scale == 0 ||
            planner.colorspace_after_scale == 0 ||
            planner.working_pixelformat != SIXEL_PIXELFORMAT_OKLABFLOAT32) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status);
}

int
test_plan_0001_resize_linear_float32(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!planner_linear_float32_valid()) {
        fprintf(stderr, "planner linear float32 resize path failed\n");
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
