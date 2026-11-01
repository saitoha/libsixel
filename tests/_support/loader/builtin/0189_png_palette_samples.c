/* SPDX-License-Identifier: MIT */
/* Compare retained palettes to source PLTE/alpha, never another loader. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms.h"
#include "src/loader-common.h"
#include "tests/data/color-numeric/palette-trns.h"
#include "loader_builtin_tga_test_common.h"

int
test_loader_0189_png_palette_samples(int argc, char **argv)
{
    edge_loader_options_t options;
    edge_frame_probe_t probe;
    edge_writer_t writer;
    /* PLTE entries over (193, 73, 29), with alpha 0, 1/3, 2/3 and 1.
     * The mixed triples before nearest rounding are (151,85,209/3) and
     * (371/3,347/3,391/3). No tie is possible with denominator 255. */
    unsigned char const png_composite[] = {
        193, 73, 29, 151, 85, 70, 124, 116, 130, 113, 157, 211
    };
    unsigned char tga[64];
    unsigned char const *alpha;
    unsigned char const *input;
    size_t length;
    size_t p;
    int mode;
    int variant;
    int first_zero;
    int i;
    int c;
    int expected;
    int index;
    SIXELSTATUS status;

    if (argc != 2) {
        return 1;
    }
    mode = atoi(argv[1]);
    if (mode == 8) {
        /* Output equality cannot reveal a representation optimization now
         * that both PNG palette routes preserve the same color/coverage. */
        return loader_png_trns_keycolor_mode() == 2 ? 0 : 1;
    }
    if (mode < 0 || mode > 7) {
        return 1;
    }
    variant = mode < 4 ? mode : mode >= 6 ? 1 : 0;
    alpha = color_pal_alpha[variant];
    input = color_pal_png[mode == 4 ? 4 : variant].bytes;
    length = color_pal_png[mode == 4 ? 4 : variant].length;
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.use_palette = 1;
    if (mode == 4) {
        options.cms_engine = SIXEL_CMS_ENGINE_BUILTIN;
    }
    if (mode == 5 || mode == 7) {
        options.set_bgcolor = 1;
        options.bgcolor[0] = 193;
        options.bgcolor[1] = 73;
        options.bgcolor[2] = 29;
    }
    if (mode >= 6) {
        /* TGA shares the converter, but keeps its legacy no-background blend.
         * Two zero entries also guard its existing transparent-index merge. */
        writer.buffer = tga;
        writer.capacity = sizeof(tga);
        writer.length = 0;
        writer.failed = 0;
        edge_tga_begin(&writer, 1, 1, 4, 32, 4, 2, 8, 0x28);
        for (i = 0; i < 4; ++i) {
            for (c = 2; c >= 0; --c) {
                edge_put_u8(&writer, color_pal_rgb[i * 3 + c]);
            }
            edge_put_u8(&writer, alpha[i]);
        }
        edge_put_bytes(&writer, color_pal_indices, sizeof(color_pal_indices));
        if (writer.failed) {
            return 1;
        }
        input = tga;
        length = writer.length;
    }
    if (edge_load_buffer_options("retained palette", input, length, &options,
                                 &probe, &status) != 0 ||
        SIXEL_FAILED(status) || probe.callback_count != 1 ||
        probe.width[0] != 4 || probe.height[0] != 2 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_PAL8 ||
        probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        probe.ncolors[0] != 4 || probe.palette_size[0] != 12 ||
        probe.rgb_size[0] != 8 || probe.mask_size[0] != 0) {
        fprintf(stderr, "retained palette: unexpected frame or status\n");
        return 1;
    }
    first_zero = -1;
    for (i = 0; i < 4; ++i) {
        if (alpha[i] == 0 && first_zero < 0 && mode != 5) {
            first_zero = i;
        }
        for (c = 0; c < 3; ++c) {
            expected = color_pal_rgb[i * 3 + c];
            if (mode == 4) {
                /* Independent stored-ICC expectation starts from source RGB,
                 * including hidden/partial colors, before any alpha policy. */
                expected = color_pal_cms[i * 3 + c];
            } else if (mode == 5) {
                expected = png_composite[i * 3 + c];
            } else if (mode >= 6 && alpha[i] < 255) {
                /* TGA compatibility control, not linear-light accuracy. */
                expected = (alpha[i] * expected + (255 - alpha[i]) *
                    (options.set_bgcolor ? options.bgcolor[c] : 0)) >> 8;
            }
            if (probe.palette[0][i * 3 + c] != expected) {
                fprintf(stderr, "palette[%d][%d]=%u expected=%d\n", i, c,
                    probe.palette[0][i * 3 + c], expected);
                return 1;
            }
        }
    }
    if (probe.transparent[0] != first_zero) {
        fprintf(stderr, "transparent=%d expected=%d\n",
                probe.transparent[0], first_zero);
        return 1;
    }
    for (p = 0; p < sizeof(color_pal_indices); ++p) {
        index = color_pal_indices[p];
        expected = alpha[index] == 0 && first_zero >= 0 ? first_zero : index;
        if (probe.rgb[0][p] != expected) {
            fprintf(stderr, "raster[%lu]=%u expected=%d\n", (unsigned long)p,
                    probe.rgb[0][p], expected);
            return 1;
        }
    }
    return 0;
}
