/*
 * Host-side work-count model of the NetBSD/x68k ite SIXEL receiver.
 *
 * The parser follows ite_dcs() in sys/arch/x68k/dev/ite.c (rev. 1.73) and
 * the band flush follows tv_sixel() in ite_tv.c (rev. 1.21): palette
 * definitions are ignored, the register number is masked to the console's
 * planes, and every data character ORs a nibble-spread pattern into a
 * one-band column buffer.  The model counts that work instead of timing it,
 * because a modern host cannot reproduce 68030 cycle costs.
 *
 * usage: ite-model MASK WIDTH HEIGHT ITE.raw DEC.raw < stream.six
 *
 * MASK is 7 for the stock eight-color console or 15 for the sixteen-color
 * patch.  ITE.raw receives the index raster produced by OR accumulation,
 * which is what the console uses for both P2=0 and P2=5.  DEC.raw receives
 * the raster produced by ordinary replacement painting, for comparison.
 * Counts are written to stdout as one JSON object.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ITE_WIDTH 768

int
main(int argc, char **argv)
{
    unsigned char *ite;
    unsigned char *dec;
    FILE *fp;
    long mask;
    long width;
    long height;
    long x;
    long y;
    long i;
    long count;
    long arg;
    long bands;
    int c;
    int state;
    int color;
    int bit;
    int in_body;
    int prev;
    unsigned long bytes;
    unsigned long data_chars;
    unsigned long repeats;
    unsigned long column_ops;
    unsigned long selectors;
    unsigned long palette_defs;
    unsigned long carriage_returns;
    unsigned long flush_words;
    unsigned long param_semicolons;

    if (argc != 6) {
        fprintf(stderr, "usage: %s MASK WIDTH HEIGHT ITE.raw DEC.raw\n",
                argv[0]);
        return 2;
    }
    mask = strtol(argv[1], NULL, 10);
    width = strtol(argv[2], NULL, 10);
    height = strtol(argv[3], NULL, 10);
    if (width <= 0 || width > MAX_ITE_WIDTH || height <= 0) {
        fprintf(stderr, "unsupported geometry\n");
        return 2;
    }
    ite = calloc((size_t)width * (size_t)height, 1);
    dec = calloc((size_t)width * (size_t)height, 1);
    if (ite == NULL || dec == NULL) {
        return 1;
    }

    bytes = data_chars = repeats = column_ops = selectors = 0;
    palette_defs = carriage_returns = flush_words = param_semicolons = 0;
    x = y = count = arg = bands = 0;
    color = 0;
    state = 0;
    in_body = 0;
    prev = 0;

    while ((c = getchar()) != EOF) {
        if (!in_body) {
            /* skip DCS P1;P2;P3 up to the final 'q' */
            if (prev == 0x1b && c == 'P') {
                in_body = 1;
                while ((c = getchar()) != EOF && c != 'q') {
                }
            }
            prev = c;
            continue;
        }
        bytes++;
        if (c == 0x1b) {
            break;
        }
        if (state == '!') {
            if (c >= '0' && c <= '9') {
                count = count * 10 + (c - '0');
                continue;
            }
            state = 0;
            if (c < '?' || c > '~') {
                continue;
            }
            repeats++;
            if (count > MAX_ITE_WIDTH - x) {
                count = MAX_ITE_WIDTH - x;
            }
            for (i = 0; i < count; i++, x++) {
                column_ops++;
                for (bit = 0; bit < 6; bit++) {
                    if (((c - '?') >> bit & 1) && x < width
                        && y + bit < height) {
                        ite[(y + bit) * width + x] |= (unsigned char)color;
                        dec[(y + bit) * width + x] = (unsigned char)color;
                    }
                }
            }
            continue;
        }
        if (state == '#' || state == '"') {
            if (c >= '0' && c <= '9') {
                arg = arg * 10 + (c - '0');
                continue;
            }
            if (c == ';') {
                param_semicolons++;
                arg = 0;
                continue;
            }
            if (state == '#') {
                if (param_semicolons != 0) {
                    palette_defs++;
                } else {
                    selectors++;
                    color = (int)(arg & mask);
                }
            }
            state = 0;
            /* fall through: c is the next sequence character */
        }
        switch (c) {
        case '!':
        case '#':
        case '"':
            state = c;
            count = arg = 0;
            param_semicolons = 0;
            break;
        case '$':
            carriage_returns++;
            x = 0;
            break;
        case '-':
            /* tv_sixel(): six rasters of 16-dot words, per plane */
            bands++;
            flush_words += 6 * ((width + 15) / 16);
            x = 0;
            y += 6;
            break;
        default:
            if (c >= '?' && c <= '~' && x < MAX_ITE_WIDTH) {
                data_chars++;
                column_ops++;
                for (bit = 0; bit < 6; bit++) {
                    if (((c - '?') >> bit & 1) && x < width
                        && y + bit < height) {
                        ite[(y + bit) * width + x] |= (unsigned char)color;
                        dec[(y + bit) * width + x] = (unsigned char)color;
                    }
                }
                x++;
            }
            break;
        }
    }
    /* the final band is flushed by ST in practice; count it once */
    if (y < height) {
        bands++;
        flush_words += 6 * ((width + 15) / 16);
    }

    fp = fopen(argv[4], "wb");
    if (fp == NULL) {
        return 1;
    }
    fwrite(ite, 1, (size_t)width * (size_t)height, fp);
    fclose(fp);
    fp = fopen(argv[5], "wb");
    if (fp == NULL) {
        return 1;
    }
    fwrite(dec, 1, (size_t)width * (size_t)height, fp);
    fclose(fp);

    printf("{\"body_bytes\": %lu, \"data_chars\": %lu, \"repeats\": %lu, "
           "\"column_ops\": %lu, \"selectors\": %lu, "
           "\"palette_defs\": %lu, \"carriage_returns\": %lu, "
           "\"bands\": %ld, \"flush_words\": %lu}\n",
           bytes, data_chars, repeats, column_ops, selectors, palette_defs,
           carriage_returns, bands, flush_words);
    free(ite);
    free(dec);
    return 0;
}
