/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <6cells.h>
#include "src/cms.h"
#include "tests/_support/planner/pipeline/playback_test_common.h"
#include "tests/_support/planner/pipeline/reserved_palette_test_common.h"
#include "tests/data/color-numeric/icc.h"

static SIXELSTATUS
set_definition(sixel_frame_t *frame, char const *name)
{
    static double const xy[] = {.3127, .329, .64, .33, .30, .60, .15, .06};
    double gamma[] = {2.2, 2.2, 2.2};
    unsigned char bytes[sizeof(color_icc)];
    sixel_frame_interface_t *iface;
    sixel_color_def_t *definition;
    SIXELSTATUS status;

    if (strcmp(name, "implicit") == 0) {
        return SIXEL_OK;
    }
    if (strcmp(name, "icc") == 0 || strcmp(name, "icc-swap") == 0) {
        memcpy(bytes, color_icc, sizeof(bytes));
        if (strcmp(name, "icc-swap") == 0) {
            /* Exchange rXYZ and bXYZ while keeping the ICC size unchanged. */
            bytes[132] = 'b';
            bytes[156] = 'r';
        }
        definition = sixel_cdef_icc(bytes, sizeof(bytes));
    } else if (strcmp(name, "power") == 0 || strcmp(name, "blue") == 0) {
        if (strcmp(name, "blue") == 0) {
            gamma[2] = 1.8;
        }
        definition = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    } else {
        definition = sixel_cdef_name(name);
    }
    if (definition == NULL) {
        return SIXEL_BAD_INPUT;
    }
    iface = sixel_frame_as_interface(frame);
    status = iface->vtbl->set_rgb_definition(iface, definition);
    sixel_cdef_free(definition);
    return status;
}

static int
test_palette_source(char const *display, char const *palette, int accept)
{
    float pixels[] = {1, 0, 0, 0, 1, 0};
    float samples[] = {1, 0, 0, 0, 1, 0};
    sixel_frame_t *frame = NULL;
    sixel_frame_t *source = NULL;
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_playback_t *playback = NULL;
    sixel_playback_options_t options;
    playback_test_buffer_t stream = {NULL, 0, 0};
    SIXELSTATUS status;
    int result = EXIT_FAILURE;

    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_new(&source, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(source, samples, 2, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1)) ||
        SIXEL_FAILED(set_definition(frame, display)) ||
        SIXEL_FAILED(set_definition(source, palette)) ||
        SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
        SIXEL_FAILED(sixel_output_new(&output, playback_test_write, &stream,
            NULL)) ||
        SIXEL_FAILED(sixel_playback_new(&playback, encoder, output, NULL,
            NULL)) ||
        SIXEL_FAILED(sixel_playback_options_init(&options, sizeof(options)))) {
        goto end;
    }
    options.palette_source = source;
    status = sixel_playback_submit(playback, frame, SIXEL_PLAYBACK_NOWAIT, 0,
                                   &options);
    if (status != (accept ? SIXEL_OK : SIXEL_BAD_ARGUMENT)) {
        fprintf(stderr, "palette source %s / %s: submit returned %#x\n",
                display, palette, (unsigned int)status);
        goto end;
    }
    if (sixel_playback_drain(playback, -1) == SIXEL_OK &&
        (stream.size != 0) == accept) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(source);
    sixel_frame_unref(frame);
    free(stream.bytes);
    return result;
}

int
test_reserved_0101(int argc, char **argv)
{
    if (argc == 4) {
        return test_palette_source(argv[1], argv[2],
                                   strcmp(argv[3], "accept") == 0);
    }
    return reserved_palette_test(4);
}
