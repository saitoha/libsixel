/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that explicit adaptive sampling accepts a public indexed frame.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

typedef struct palette_output_counter {
    int calls;
    int bytes;
} palette_output_counter_t;

static int
palette_output_write(char *data, int size, void *priv)
{
    palette_output_counter_t *counter;

    counter = (palette_output_counter_t *)priv;
    if (counter == NULL) {
        return 0;
    }
    counter->calls += 1;
    counter->bytes += size;
    (void)data;
    return size;
}

static int
indexed_adaptive_encode_is_valid(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    palette_output_counter_t counter;
    unsigned char pixels[256];
    unsigned char palette[256 * 3];
    char const *sink_path;
    int index;

    status = SIXEL_FALSE;
    allocator = NULL;
    encoder = NULL;
    frame = NULL;
    output = NULL;
    counter.calls = 0;
    counter.bytes = 0;
#if defined(_WIN32)
    sink_path = "NUL";
#else
    sink_path = "/dev/null";
#endif

    for (index = 0; index < 256; ++index) {
        pixels[index] = (unsigned char)index;
        palette[index * 3 + 0] = (unsigned char)index;
        palette[index * 3 + 1] = (unsigned char)(255 - index);
        palette[index * 3 + 2] = (unsigned char)(index ^ 0x55);
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_frame_init_borrowed(frame,
                                       pixels,
                                       16,
                                       16,
                                       SIXEL_PIXELFORMAT_PAL8,
                                       palette,
                                       256);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_output_new(&output,
                              palette_output_write,
                              &counter,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, sink_path);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADS, "1");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "16");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(
        encoder,
        SIXEL_OPTFLAG_QUANTIZE_MODEL,
        "auto:sampling_policy=adaptive-grid");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_QUANTIZE_MODEL,
                                  "heckbert");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (SIXEL_SUCCEEDED(status) &&
            (counter.calls <= 0 || counter.bytes <= 0)) {
        status = SIXEL_LOGIC_ERROR;
    }

cleanup:
    sixel_output_unref(output);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return SIXEL_SUCCEEDED(status);
}

int
test_palette_0037_palette_sampling_indexed_frame(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!indexed_adaptive_encode_is_valid()) {
        fprintf(stderr, "indexed adaptive sampling encode failed\n");
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
