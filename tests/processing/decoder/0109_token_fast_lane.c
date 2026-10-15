/* SPDX-License-Identifier: MIT */
/*
 * Selections, repeats and carriage returns consumed inside a band run must
 * decode exactly as the general parser decodes them. Zero-padding every
 * "#" and "!" number past the fast lane's digit limit keeps each stream's
 * meaning but sends those tokens through the general parser, so the padded
 * stream is the reference for the original one.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/sixel_decode_pixels.h"

static char const *const bodies[] = {
    /* Mixed selections, repeats, carriage returns and band changes. */
    ("\"1;1;40;12#1;2;100;0;0#2;2;0;100;0#3;2;0;0;100"
     "#1!5~#2AB!3?C#3!7N$#2!20@#1~~-#3!40~$#1!10?!12F"),
    /* Repeats reaching past the allocated width grow the canvas. */
    "#1!30~#2!5N$#3~~~!2000A",
    /* ESC abandons a selection; the image ends there. */
    "\"1;1;20;6#1~~#200\033\\",
    /* Zero and empty repeat counts, a wide register, a definition. */
    "#1~~!0~!1~!~#300~~#5;2;1;2;3~~#1~",
    /* An oversized repeat is an error either way. */
    "#1~~!70000~",
    /* A trusted raster clips repeats on both edges. */
    "\"1;1;10;8#1!12~$#2!3N-#3!12~",
    /* Extents after a carriage return to a shorter column. */
    "\"1;1;40;12#1!30?~$#2~-#3~",
    /* Without a raster the extents alone decide the returned width. */
    "#1!30?~$#2~-#3~",
    /* The largest byte register, register zero, the first wide one. */
    "#255!4~#0!4~#256~~#7~",
    /* A repeat count carried across a selection. */
    "#1~!5#2~~",
    /* A repeat followed by a carriage return before its data byte. */
    "\"1;1;20;12#2!3$~~#3!4-~",
    /* Repeats ending exactly at and one past the declared width. */
    "\"1;1;10;6#1~~~~~!5~$#2~~~~~!6~",
    /* Tokens cut by the end of input (see build()). */
    "#1~~!",
    "#1~~#2",
    "#1~~!5",
};

/* The last bodies are written without a string terminator. */
#define CUT_BODIES 3

/*
 * Copy src into dst, writing each number after '#' or '!' with six digits.
 * Before each '$' the current register is selected again, also padded: the
 * selection changes nothing, but it ends the fast lane so the carriage
 * return itself also reaches the general parser.
 */
static int
pad_numbers(char const *src, char *dst, size_t size)
{
    size_t n;
    int value;
    int current;
    int selection;

    n = 0;
    current = -1;
    while (*src != '\0') {
        if (n + 16 >= size) {
            return -1;
        }
        if (*src == '$' && current >= 0) {
            n += (size_t)sprintf(dst + n, "#%06d", current);
        }
        dst[n++] = *src;
        if ((*src == '#' || *src == '!') &&
                src[1] >= '0' && src[1] <= '9') {
            selection = *src++ == '#';
            value = 0;
            while (*src >= '0' && *src <= '9') {
                value = value * 10 + (*src++ - '0');
            }
            if (selection) {
                current = value;
            }
            n += (size_t)sprintf(dst + n, "%06d", value);
            continue;
        }
        ++src;
    }
    dst[n] = '\0';
    return (int)n;
}

static int
build(char const *body, int ormode, int pad, int cut,
      unsigned char *out, size_t size)
{
    char padded[512];
    int length;

    if (pad) {
        if (pad_numbers(body, padded, sizeof(padded)) < 0) {
            return -1;
        }
        body = padded;
    }
    length = snprintf((char *)out, size, "\033P0;%dq%s%s",
                      ormode ? 5 : 1, body, cut ? "" : "\033\\");
    return length < 0 || (size_t)length >= size ? -1 : length;
}

static int
same_raw(unsigned char *a, int alen, unsigned char *b, int blen,
         unsigned int options)
{
    unsigned char *pix[2] = { NULL, NULL };
    unsigned char *mask[2] = { NULL, NULL };
    unsigned char *pal[2] = { NULL, NULL };
    unsigned int flags[2] = { 0, 0 };
    SIXELSTATUS status[2];
    int w[2] = { 0, 0 };
    int h[2] = { 0, 0 };
    int nc[2] = { 0, 0 };
    int ok;

    status[0] = sixel_decode_raw_with_options_mask(a, alen, options,
        &pix[0], &mask[0], &w[0], &h[0], &pal[0], &nc[0], &flags[0], NULL);
    status[1] = sixel_decode_raw_with_options_mask(b, blen, options,
        &pix[1], &mask[1], &w[1], &h[1], &pal[1], &nc[1], &flags[1], NULL);
    ok = status[0] == status[1];
    if (ok && SIXEL_SUCCEEDED(status[0])) {
        ok = w[0] == w[1] && h[0] == h[1] && nc[0] == nc[1] &&
            flags[0] == flags[1] &&
            memcmp(pix[0], pix[1], (size_t)w[0] * (size_t)h[0]) == 0 &&
            memcmp(mask[0], mask[1], (size_t)w[0] * (size_t)h[0]) == 0 &&
            memcmp(pal[0], pal[1], (size_t)nc[0] * 3u) == 0;
    }
    free(pix[0]); free(pix[1]);
    free(mask[0]); free(mask[1]);
    free(pal[0]); free(pal[1]);
    return ok;
}

static int
same_direct(unsigned char *a, int alen, unsigned char *b, int blen,
            unsigned int options)
{
    unsigned char *pix[2] = { NULL, NULL };
    unsigned char *pal[2] = { NULL, NULL };
    unsigned int flags[2] = { 0, 0 };
    SIXELSTATUS status[2];
    int w[2] = { 0, 0 };
    int h[2] = { 0, 0 };
    int nc[2] = { 0, 0 };
    int ok;

    status[0] = sixel_decode_direct_with_options(a, alen, options,
        &pix[0], &w[0], &h[0], &pal[0], &nc[0], &flags[0], NULL);
    status[1] = sixel_decode_direct_with_options(b, blen, options,
        &pix[1], &w[1], &h[1], &pal[1], &nc[1], &flags[1], NULL);
    ok = status[0] == status[1];
    if (ok && SIXEL_SUCCEEDED(status[0])) {
        ok = w[0] == w[1] && h[0] == h[1] && nc[0] == nc[1] &&
            flags[0] == flags[1] &&
            memcmp(pix[0], pix[1], (size_t)w[0] * (size_t)h[0] * 4u) == 0;
    }
    free(pix[0]); free(pix[1]);
    free(pal[0]); free(pal[1]);
    return ok;
}

int
test_decoder_0109(int argc, char **argv)
{
    unsigned char fast[512];
    unsigned char general[512];
    unsigned int options;
    size_t i;
    int flen;
    int glen;
    int ormode;
    int trust;
    int cut;

    (void)argc;
    (void)argv;
    for (i = 0; i < sizeof(bodies) / sizeof(bodies[0]); ++i) {
        cut = i + CUT_BODIES >= sizeof(bodies) / sizeof(bodies[0]);
        for (ormode = 0; ormode < 2; ++ormode) {
            flen = build(bodies[i], ormode, 0, cut, fast, sizeof(fast));
            glen = build(bodies[i], ormode, 1, cut, general,
                         sizeof(general));
            if (flen < 0 || glen < 0) {
                fprintf(stderr, "stream %u does not fit\n", (unsigned)i);
                return EXIT_FAILURE;
            }
            for (trust = 0; trust < 2; ++trust) {
                options = trust ?
                    SIXEL_DECODE_PIXELS_OPTION_TRUST_RASTER_SIZE : 0u;
                if (!same_raw(fast, flen, general, glen, options) ||
                        !same_direct(fast, flen, general, glen, options)) {
                    fprintf(stderr, "stream %u ormode %d trust %d differs\n",
                            (unsigned)i, ormode, trust);
                    return EXIT_FAILURE;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}
