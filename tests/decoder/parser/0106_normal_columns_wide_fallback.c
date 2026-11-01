/* SPDX-License-Identifier: MIT */
/* Wide selection retains earlier narrow paint at all three output depths. */
#include <stdlib.h>
#include <sixel.h>

SIXELSTATUS sixel_decode_wide(unsigned char *, int, unsigned short **,
    int *, int *, unsigned char **, int *, sixel_allocator_t *);

int
test_decoder_0096(int argc, char **argv)
{
    unsigned char input[] =
        "\033Pq\"1;1;3;6#1;2;100;0;0~#256;2;0;100;0~"
        "#65535;2;0;0;100~\033\\";
    unsigned char *raw;
    unsigned short *wide;
    unsigned char *palette;
    unsigned char *rgba;
    SIXELSTATUS status;
    int width;
    int height;
    int ncolors;
    int x;
    int y;
    int ok;

    (void)argc;
    (void)argv;
    raw = palette = rgba = NULL;
    wide = NULL;
    ok = 0;
    status = sixel_decode_raw(input, sizeof(input) - 1, &raw,
        &width, &height, &palette, &ncolors, NULL);
    if (SIXEL_FAILED(status) || width != 3 || height != 6 ||
            ncolors != 65536) { goto done; }
    for (y = 0; y < 6; ++y) {
        if (raw[y * 3] != 1 || raw[y * 3 + 1] != 0 ||
                raw[y * 3 + 2] != 255) { goto done; }
    }
    free(palette); palette = NULL;
    status = sixel_decode_wide(input, sizeof(input) - 1, &wide,
        &width, &height, &palette, &ncolors, NULL);
    if (SIXEL_FAILED(status) || width != 3 || height != 6 ||
            ncolors != 65536) { goto done; }
    for (y = 0; y < 6; ++y) {
        if (wide[y * 3] != 1 || wide[y * 3 + 1] != 256 ||
                wide[y * 3 + 2] != 65535) { goto done; }
    }
    status = sixel_decode_direct(input, sizeof(input) - 1, &rgba,
                                 &width, &height, NULL);
    if (SIXEL_FAILED(status) || width != 3 || height != 6) { goto done; }
    for (y = 0; y < 6; ++y) {
        for (x = 0; x < 3; ++x) {
            if (rgba[(y * 3 + x) * 4] != (x == 0 ? 255 : 0) ||
                    rgba[(y * 3 + x) * 4 + 1] != (x == 1 ? 255 : 0) ||
                    rgba[(y * 3 + x) * 4 + 2] != (x == 2 ? 255 : 0) ||
                    rgba[(y * 3 + x) * 4 + 3] != 255) { goto done; }
        }
    }
    ok = 1;
done:
    free(raw); free(wide); free(palette); free(rgba);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
