/*
 * SPDX-License-Identifier: MIT
 *
 * Compare OR-mode pipeline output with the serial OR-mode body contract.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/dither.h"
#include "src/encoder-core-private.h"

static int
test_encoder_core_ormode_pipeline_write(char *data, int size, void *priv)
{
    (void)data;
    (void)size;
    (void)priv;

    return 0;
}

int
test_encoder_core_0005_ormode_pipeline_body_match(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_dither_t *serial_dither;
    sixel_dither_t *pipeline_dither;
    sixel_output_t *serial_output;
    sixel_output_t *pipeline_output;
    sixel_index_t *indexes;
    unsigned char palette[12];
    enum {
        width = 4,
        height = 13,
        depth = 3,
        colors = 4
    };
    unsigned char pixels[width * height * depth];
    size_t pixel_offset;
    int x;
    int y;
    int color;
    int ok;

    (void)argc;
    (void)argv;

    serial_dither = NULL;
    pipeline_dither = NULL;
    serial_output = NULL;
    pipeline_output = NULL;
    indexes = NULL;
    ok = 0;

    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 255;
    palette[4] = 0;
    palette[5] = 0;
    palette[6] = 0;
    palette[7] = 255;
    palette[8] = 0;
    palette[9] = 0;
    palette[10] = 0;
    palette[11] = 255;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            color = (x + y) & 3;
            pixel_offset = ((size_t)y * (size_t)width + (size_t)x) * depth;
            pixels[pixel_offset + 0U] = palette[color * 3 + 0];
            pixels[pixel_offset + 1U] = palette[color * 3 + 1];
            pixels[pixel_offset + 2U] = palette[color * 3 + 2];
        }
    }

    status = sixel_dither_new(&serial_dither, colors, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_dither_new(&pipeline_dither, colors, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_dither_set_palette(serial_dither, palette);
    sixel_dither_set_pixelformat(serial_dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(serial_dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_diffusion_scan(serial_dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_optimize_palette(serial_dither, 0);
    serial_dither->force_palette = 1;

    sixel_dither_set_palette(pipeline_dither, palette);
    sixel_dither_set_pixelformat(pipeline_dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(pipeline_dither, SIXEL_DIFFUSE_NONE);
    sixel_dither_set_diffusion_scan(pipeline_dither, SIXEL_SCAN_RASTER);
    sixel_dither_set_optimize_palette(pipeline_dither, 0);
    pipeline_dither->force_palette = 1;
    pipeline_dither->pipeline_parallel_active = 1;
    pipeline_dither->pipeline_band_height = 6;
    pipeline_dither->pipeline_band_overlap = 0;
    pipeline_dither->pipeline_dither_threads = 2;
    pipeline_dither->pipeline_pin_threads = 0;

    status = sixel_output_new(&serial_output,
                              test_encoder_core_ormode_pipeline_write,
                              NULL,
                              NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&pipeline_output,
                              test_encoder_core_ormode_pipeline_write,
                              NULL,
                              NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    serial_output->ormode = 1;
    pipeline_output->ormode = 1;

    indexes = sixel_dither_apply_palette(serial_dither,
                                         pixels,
                                         width,
                                         height);
    if (indexes == NULL) {
        fprintf(stderr, "serial palette application failed\n");
        goto end;
    }
    status = sixel_encode_body_ormode(indexes,
                                      width,
                                      height,
                                      palette,
                                      serial_dither->ncolors,
                                      serial_dither->keycolor,
                                      serial_output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "serial OR-mode body returned %04x\n", status);
        goto end;
    }

    status = sixel_encode_body_ormode_pipeline(pixels,
                                               width,
                                               height,
                                               palette,
                                               pipeline_dither,
                                               pipeline_output,
                                               2);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "OR-mode pipeline returned %04x\n", status);
        goto end;
    }

    if (serial_output->pos != pipeline_output->pos) {
        fprintf(stderr,
                "OR-mode pipeline body size is %d, expected %d\n",
                pipeline_output->pos,
                serial_output->pos);
        goto end;
    }
    if (memcmp(serial_output->buffer,
               pipeline_output->buffer,
               (size_t)serial_output->pos) != 0) {
        fprintf(stderr, "OR-mode pipeline body bytes differ\n");
        goto end;
    }

    ok = 1;

end:
    if (indexes != NULL && serial_dither != NULL) {
        sixel_allocator_free(serial_dither->allocator, indexes);
    }
    if (serial_output != NULL) {
        sixel_output_unref(serial_output);
    }
    if (pipeline_output != NULL) {
        sixel_output_unref(pipeline_output);
    }
    if (serial_dither != NULL) {
        sixel_dither_unref(serial_dither);
    }
    if (pipeline_dither != NULL) {
        sixel_dither_unref(pipeline_dither);
    }

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
