/*
 * SPDX-License-Identifier: MIT
 *
 * Automatic sampling must keep the palette of a PAL8 frame passed through
 * sixel_encoder_encode_frame(): re-encoding decoded indices reproduces the
 * first generation's RGB pixels at every worker budget.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#define PAL8_RETENTION_WIDTH 64
#define PAL8_RETENTION_HEIGHT 48

typedef struct pal8_retention_buffer {
    unsigned char *data;
    size_t size;
    size_t capacity;
} pal8_retention_buffer_t;

static int
pal8_retention_append(char *data, int size, void *priv)
{
    pal8_retention_buffer_t *buffer;
    unsigned char *grown;
    size_t capacity;

    buffer = (pal8_retention_buffer_t *)priv;
    if (buffer->size + (size_t)size > buffer->capacity) {
        capacity = (buffer->size + (size_t)size) * 2u;
        grown = (unsigned char *)realloc(buffer->data, capacity);
        if (grown == NULL) {
            return -1;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->size, data, (size_t)size);
    buffer->size += (size_t)size;
    return size;
}

/* Encode one frame; the caller frees buffer->data. */
static int
pal8_retention_encode(unsigned char *pixels, int pixelformat,
                      unsigned char *palette, int ncolors, char const *threads,
                      pal8_retention_buffer_t *buffer)
{
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    memset(buffer, 0, sizeof(*buffer));
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADS,
                                      threads);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_output_new(&output, pal8_retention_append, buffer,
                                  NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_new(&frame, NULL);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_frame_init_borrowed(frame, pixels,
                                           PAL8_RETENTION_WIDTH,
                                           PAL8_RETENTION_HEIGHT,
                                           pixelformat, palette, ncolors);
    }
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_encoder_encode_frame(encoder, frame, output);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    return SIXEL_SUCCEEDED(status);
}

/* Decode to indices and palette, then expand to RGB888. */
static int
pal8_retention_decode(pal8_retention_buffer_t const *buffer,
                      unsigned char **indices, unsigned char **palette,
                      int *ncolors, unsigned char *rgb)
{
    SIXELSTATUS status;
    int width;
    int height;
    int i;
    int index;

    status = sixel_decode_raw(buffer->data, (int)buffer->size, indices,
                              &width, &height, palette, ncolors, NULL);
    if (SIXEL_FAILED(status) || width != PAL8_RETENTION_WIDTH ||
            height != PAL8_RETENTION_HEIGHT) {
        return 0;
    }
    for (i = 0; i < width * height; i++) {
        index = (*indices)[i];
        if (index >= *ncolors) {
            return 0;
        }
        memcpy(rgb + i * 3, *palette + index * 3, 3u);
    }
    return 1;
}

static int
pal8_retention_holds(char const *threads)
{
    unsigned char source[PAL8_RETENTION_WIDTH * PAL8_RETENTION_HEIGHT * 3];
    unsigned char first[PAL8_RETENTION_WIDTH * PAL8_RETENTION_HEIGHT * 3];
    unsigned char second[PAL8_RETENTION_WIDTH * PAL8_RETENTION_HEIGHT * 3];
    pal8_retention_buffer_t gen1;
    pal8_retention_buffer_t gen2;
    unsigned char *indices1;
    unsigned char *palette1;
    unsigned char *indices2;
    unsigned char *palette2;
    int ncolors1;
    int ncolors2;
    int x;
    int y;
    int ok;

    indices1 = palette1 = indices2 = palette2 = NULL;
    ncolors1 = ncolors2 = 0;
    memset(&gen1, 0, sizeof(gen1));
    memset(&gen2, 0, sizeof(gen2));
    /* A smooth two-axis ramp needs many palette entries. */
    for (y = 0; y < PAL8_RETENTION_HEIGHT; y++) {
        for (x = 0; x < PAL8_RETENTION_WIDTH; x++) {
            source[(y * PAL8_RETENTION_WIDTH + x) * 3 + 0] =
                (unsigned char)(x * 4);
            source[(y * PAL8_RETENTION_WIDTH + x) * 3 + 1] =
                (unsigned char)(y * 5);
            source[(y * PAL8_RETENTION_WIDTH + x) * 3 + 2] =
                (unsigned char)((x + y) * 2);
        }
    }
    ok = pal8_retention_encode(source, SIXEL_PIXELFORMAT_RGB888, NULL, -1,
                               threads, &gen1)
        && pal8_retention_decode(&gen1, &indices1, &palette1, &ncolors1,
                                 first)
        && pal8_retention_encode(indices1, SIXEL_PIXELFORMAT_PAL8,
                                 palette1, ncolors1, threads, &gen2)
        && pal8_retention_decode(&gen2, &indices2, &palette2, &ncolors2,
                                 second)
        && memcmp(first, second, sizeof(first)) == 0;
    if (!ok) {
        fprintf(stderr, "PAL8 retention failed with %s threads\n", threads);
    }
    free(indices1);
    free(palette1);
    free(indices2);
    free(palette2);
    free(gen1.data);
    free(gen2.data);
    return ok;
}

int
test_palette_0081_sampling_auto_pal8_retention(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!pal8_retention_holds("1") || !pal8_retention_holds("2") ||
            !pal8_retention_holds("8")) {
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
