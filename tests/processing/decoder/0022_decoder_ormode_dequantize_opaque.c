/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that OR mode stays opaque through the dequantize decode path.
 *
 * OR mode composes a palette index from bit planes, so a cell that no plane
 * touches still carries palette index 0 as an ordinary color.  The dequantize
 * path in sixel_decoder_decode_pixels() derives alpha from the decoder paint
 * mask, and the bit-plane store only marks cells whose sixel bit was set.
 * Without the OR mode fill those index-0 cells came back as transparent black
 * while the direct path returned palette index 0, which showed up as black
 * speckles over img2sixel -O output.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

/*
 * 8x6 OR mode image.  Plane #1 covers the left half, so x < 4 composes index
 * 1 (green) and x >= 4 stays index 0 (red) without ever being marked painted.
 */
static unsigned char g_ormode_dequantize_payload[] =
    "\033P7;5q\"1;1;8;6"
    "#0;2;100;0;0"
    "#1;2;0;100;0"
    "#1!4~!4?\033\\";

static char const *g_ormode_dequantize_methods[] = {
    "selective_blur:threshold=24",
    "k_undither"
};

static int
ormode_dequantize_check(char const *method, sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_decoder_t *decoder;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    unsigned char const *pixel;
    size_t index;
    size_t npixels;
    int ok;

    decoder = NULL;
    ok = 0;
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGBA8888;

    status = sixel_decoder_new(&decoder, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_decoder_new() failed\n");
        goto end;
    }
    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_DEQUANTIZE, method);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "dequantize method %s was rejected\n", method);
        goto end;
    }
    status = sixel_decoder_decode_pixels(
        decoder,
        g_ormode_dequantize_payload,
        sizeof(g_ormode_dequantize_payload) - 1U,
        &options,
        &result);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "decode failed for %s\n", method);
        goto end;
    }
    if (result.width != 8 || result.height != 6) {
        fprintf(stderr,
                "%s: unexpected dimensions %dx%d\n",
                method,
                result.width,
                result.height);
        goto end;
    }

    npixels = (size_t)result.width * (size_t)result.height;
    for (index = 0u; index < npixels; ++index) {
        if (result.pixels[index * 4u + 3u] != 255u) {
            fprintf(stderr,
                    "%s: pixel %lu is not opaque (alpha %u)\n",
                    method,
                    (unsigned long)index,
                    (unsigned int)result.pixels[index * 4u + 3u]);
            goto end;
        }
    }
    if ((result.flags & SIXEL_DECODE_PIXELS_RESULT_ALPHA_OPAQUE) == 0U) {
        fprintf(stderr, "%s: ALPHA_OPAQUE was not reported\n", method);
        goto end;
    }

    /* Interior samples stay clear of the 3x3 kernel reaching the seam. */
    pixel = result.pixels + ((size_t)3 * 8u + 7u) * 4u;
    if (pixel[0] != 255u || pixel[1] != 0u || pixel[2] != 0u) {
        fprintf(stderr,
                "%s: unpainted OR mode cell is %u,%u,%u, expected 255,0,0\n",
                method,
                (unsigned int)pixel[0],
                (unsigned int)pixel[1],
                (unsigned int)pixel[2]);
        goto end;
    }
    pixel = result.pixels + ((size_t)3 * 8u + 0u) * 4u;
    if (pixel[0] != 0u || pixel[1] != 255u || pixel[2] != 0u) {
        fprintf(stderr,
                "%s: painted OR mode cell is %u,%u,%u, expected 0,255,0\n",
                method,
                (unsigned int)pixel[0],
                (unsigned int)pixel[1],
                (unsigned int)pixel[2]);
        goto end;
    }

    ok = 1;

end:
    if (result.pixels != NULL) {
        sixel_allocator_free(allocator, result.pixels);
    }
    if (decoder != NULL) {
        sixel_decoder_unref(decoder);
    }
    return ok;
}

int
test_decoder_0022_decoder_ormode_dequantize_opaque(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    size_t i;
    int ok;

    (void)argc;
    (void)argv;

    allocator = NULL;
    ok = 1;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "sixel_allocator_new() failed\n");
        return EXIT_FAILURE;
    }

    for (i = 0u;
            i < sizeof(g_ormode_dequantize_methods) /
                sizeof(g_ormode_dequantize_methods[0]);
            ++i) {
        if (!ormode_dequantize_check(g_ormode_dequantize_methods[i],
                                     allocator)) {
            ok = 0;
            break;
        }
    }

    sixel_allocator_unref(allocator);

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
