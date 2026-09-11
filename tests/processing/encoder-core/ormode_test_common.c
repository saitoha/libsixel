/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder-core-private.h"
#include "src/dither.h"
#include "ormode_test_common.h"

static int
or_test_discard(char *data, int size, void *opaque)
{
    (void)data;
    (void)opaque;
    return size;
}

int
test_or_encode(unsigned char const *source, int width, int height,
               int colors, int policy, int left, int top,
               char const *expected_body)
{
    sixel_output_t *output;
    sixel_allocator_t *allocator;
    sixel_index_t indexes[512];
    unsigned char palette[768];
    unsigned char *decoded;
    unsigned char *decoded_palette;
    char stream[16384];
    char *body;
    char *next;
    int i;
    int field;
    int x;
    int y;
    int w;
    int h;
    int n;
    int length;
    int expected;
    int ok;
    SIXELSTATUS status;

    output = NULL;
    allocator = NULL;
    decoded = NULL;
    decoded_palette = NULL;
    ok = 0;
    if (width * height > 512 || colors > 256) {
        return EXIT_FAILURE;
    }
    for (i = 0; i < width * height; i++) {
        indexes[i] = source[i];
    }
    for (i = 0; i < colors; i++) {
        palette[i * 3] = (unsigned char)i;
        palette[i * 3 + 1] = (unsigned char)(255 - i);
        palette[i * 3 + 2] = 0;
    }
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&output, or_test_discard, NULL, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    output->encode_policy = policy;
    output->transparent_offset_left = left;
    output->transparent_offset_top = top;
    status = sixel_encode_body_ormode(indexes, width, height, palette,
                                      colors, -1, output);
    if (SIXEL_FAILED(status) || output->pos >= 12000) {
        goto end;
    }
    output->buffer[output->pos] = '\0';
    body = (char *)output->buffer;
    /* Skip exactly the palette definitions, retaining all paint selectors. */
    for (i = 0; i < colors; i++) {
        if (*body != '#') {
            goto end;
        }
        (void)strtol(body + 1, &next, 10);
        for (field = 0; field < 4; field++) {
            if (*next != ';') {
                goto end;
            }
            (void)strtol(next + 1, &next, 10);
        }
        body = next;
    }
    if (expected_body != NULL && strcmp(body, expected_body) != 0) {
        fprintf(stderr, "OR body [%s], expected [%s]\n",
                body, expected_body);
        goto end;
    }
    /* The raster header supplies geometry even when every plane is empty. */
    length = snprintf(stream, sizeof(stream),
                      "\033P7;5q\"1;1;%d;%d%s\033\\",
                      width + left, height + top, output->buffer);
    status = sixel_decode_raw((unsigned char *)stream, length, &decoded,
                              &w, &h, &decoded_palette, &n, allocator);
    if (SIXEL_FAILED(status) || w != width + left || h != height + top) {
        goto end;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            expected = x < left || y < top ? 0 :
                source[(y - top) * width + x - left];
            if (decoded[y * w + x] != expected) {
                fprintf(stderr, "OR index at %d,%d is %u, expected %d\n",
                        x, y, decoded[y * w + x], expected);
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (allocator != NULL) {
        sixel_allocator_free(allocator, decoded);
        sixel_allocator_free(allocator, decoded_palette);
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
or_pipeline_write(char *data, int size, void *priv)
{
    (void)data;
    (void)size;
    (void)priv;

    return 0;
}

int
test_or_pipeline(int policy, int left, int top)
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
                              or_pipeline_write,
                              NULL,
                              NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_output_new(&pipeline_output,
                              or_pipeline_write,
                              NULL,
                              NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    serial_output->encode_policy = policy;
    pipeline_output->encode_policy = policy;
    serial_output->transparent_offset_left = left;
    serial_output->transparent_offset_top = top;
    pipeline_output->transparent_offset_left = left;
    pipeline_output->transparent_offset_top = top;
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
