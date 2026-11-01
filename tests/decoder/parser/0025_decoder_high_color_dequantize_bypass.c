/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that high color streams bypass the dequantize decode path.
 *
 * High color output (img2sixel -I) redefines color registers while painting,
 * so an index plane plus one palette snapshot cannot describe it: every pixel
 * would resolve against the last definition of its register.  The dequantize
 * path in sixel_decoder_decode_pixels() works on exactly that indexed form,
 * so it has to detect the redefinition and decode directly instead.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

/*
 * Two pixels that share register 0.  The register is red while the left pixel
 * is painted and blue while the right one is, so an indexed decode collapses
 * both to blue.
 */
static unsigned char g_high_color_payload[] =
    "\033Pq\"1;1;2;1"
    "#0;2;100;0;0@"
    "#0;2;0;0;100@"
    "\033\\";

static int
high_color_check(char const *method, sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_decoder_t *decoder;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
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
    if (method != NULL) {
        status = sixel_decoder_setopt(decoder,
                                      SIXEL_OPTFLAG_DEQUANTIZE,
                                      method);
        if (SIXEL_FAILED(status)) {
            fprintf(stderr, "dequantize method %s was rejected\n", method);
            goto end;
        }
    }
    status = sixel_decoder_decode_pixels(decoder,
                                         g_high_color_payload,
                                         sizeof(g_high_color_payload) - 1U,
                                         &options,
                                         &result);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "decode failed for %s\n",
                method == NULL ? "(none)" : method);
        goto end;
    }
    if (result.width != 2 || result.height != 1) {
        fprintf(stderr,
                "%s: unexpected dimensions %dx%d\n",
                method == NULL ? "(none)" : method,
                result.width,
                result.height);
        goto end;
    }
    if ((result.flags & SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED) == 0U) {
        fprintf(stderr,
                "%s: PALETTE_REDEFINED was not reported\n",
                method == NULL ? "(none)" : method);
        goto end;
    }
    if (result.pixels[0] != 255u || result.pixels[1] != 0u ||
            result.pixels[2] != 0u) {
        fprintf(stderr,
                "%s: left pixel is %u,%u,%u, expected 255,0,0\n",
                method == NULL ? "(none)" : method,
                (unsigned int)result.pixels[0],
                (unsigned int)result.pixels[1],
                (unsigned int)result.pixels[2]);
        goto end;
    }
    if (result.pixels[4] != 0u || result.pixels[5] != 0u ||
            result.pixels[6] != 255u) {
        fprintf(stderr,
                "%s: right pixel is %u,%u,%u, expected 0,0,255\n",
                method == NULL ? "(none)" : method,
                (unsigned int)result.pixels[4],
                (unsigned int)result.pixels[5],
                (unsigned int)result.pixels[6]);
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
test_decoder_0023_decoder_high_color_dequantize_bypass(int argc, char **argv)
{
    static char const *methods[] = {
        NULL,
        "selective_blur:threshold=24",
        "k_undither"
    };
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

    for (i = 0u; i < sizeof(methods) / sizeof(methods[0]); ++i) {
        if (!high_color_check(methods[i], allocator)) {
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
