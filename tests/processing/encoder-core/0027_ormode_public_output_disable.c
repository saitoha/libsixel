/* SPDX-License-Identifier: MIT */
/* The public output setter can turn OR mode back off. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "ormode_test_common.h"

/* Keep the tiny complete wire stream, including any writer flushes. */
static int
or_toggle_write(char *data, int size, void *opaque)
{
    char *stream;
    size_t used;

    stream = (char *)opaque;
    used = strlen(stream);
    if (size < 0 || used + (size_t)size >= 1024U) {
        return -1;
    }
    memcpy(stream + used, data, (size_t)size);
    stream[used + (size_t)size] = '\0';
    return size;
}

int
test_or_enc_0027(int argc, char **argv)
{
    sixel_output_t *output;
    sixel_dither_t *dither;
    unsigned char palette[6] = { 0, 0, 0, 255, 255, 255 };
    unsigned char pixels[1] = { 1 };
    char stream[1024];
    int ok;

    (void)argc;
    (void)argv;
    output = NULL;
    dither = NULL;
    stream[0] = '\0';
    ok = 0;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, or_toggle_write,
                                           stream, NULL))) {
        goto end;
    }
    sixel_dither_set_palette(dither, palette);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_PAL8);
    sixel_output_set_ormode(output, 1);
    sixel_output_set_ormode(output, 0);
    if (SIXEL_FAILED(sixel_encode(pixels, 1, 1, 1, dither, output))) {
        goto end;
    }
    ok = stream[0] == '\033' && strstr(stream, "P7;5q") == NULL;
end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
