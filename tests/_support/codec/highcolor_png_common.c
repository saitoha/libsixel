/* SPDX-License-Identifier: MIT */
/* Test the actual file frontend, including staging, readback and PNG tags. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/compat_stub.h"
#include "src/encoder.h"
#include "src/dither.h"
#include "src/cms.h"
#include "tests/_support/loader/builtin/loader_builtin_memory_test_common.h"
#include "tests/writer/png/color_test_common.h"

/* Independent D65 sRGB to Display P3 matrix and analytic transfer curve.
 * Filter candidates whose key/percent rounding is within .01 units of a
 * tie. That margin exceeds the <.002-unit float conversion budget. */
static int
hp_samples(unsigned char *input, unsigned char *expected, int p3)
{
    static double const matrix[3][3] = {
        {.822461968714362, .177538031285638, 0},
        {.033194198850962, .966805801149038, 0},
        {.017082630721120, .072397440663963, .910519928614917}
    };
    unsigned char used[32768];
    unsigned char rgb[3];
    unsigned char want[3];
    double linear[3];
    double value;
    double wire;
    double byte;
    int i;
    int c;
    int j;
    int key;
    int safe;
    int count;

    memset(used, 0, sizeof(used));
    count = 0;
    for (i = 0; i < 1024 && count < 320; ++i) {
        rgb[0] = (unsigned char)((i % 32) * 8 + 2);
        rgb[1] = (unsigned char)((i / 32) * 8 + 2);
        rgb[2] = 61;
        for (c = 0; c < 3; ++c) {
            value = rgb[c] / 255.0;
            linear[c] = value <= .04045 ? value / 12.92 :
                pow((value + .055) / 1.055, 2.4);
        }
        key = 0;
        safe = 1;
        for (c = 0; c < 3; ++c) {
            value = rgb[c] / 255.0;
            if (p3) {
                value = 0;
                for (j = 0; j < 3; ++j) {
                    value += matrix[c][j] * linear[j];
                }
                value = value <= .0031308 ? 12.92 * value :
                    1.055 * pow(value, 1.0 / 2.4) - .055;
            }
            if (!isfinite(value)) {
                return 0;
            }
            wire = value * 100;
            byte = value * 255;
            if (fabs(wire - floor(wire) - .5) < .01 ||
                    fabs(byte - floor(byte) - .5) < .01) {
                safe = 0;
            }
            key = (key << 5) | ((int)(byte + .5) >> 3);
            want[c] = (unsigned char)(((int)(wire + .5) * 255 + 50) / 100);
        }
        if (!safe || used[key]) {
            continue;
        }
        used[key] = 1;
        memcpy(input + count * 3, rgb, 3);
        memcpy(expected + count * 3, want, 3);
        ++count;
    }
    return count == 320;
}

int
test_highcolor_png(int mode, char const *directory)
{
    /* Independent HLS field/reconstruction oracle for sRGB primaries
     * converted to P3. See the shared PNG snapshot rounding contract. */
    static unsigned char const primaries[9] = {
        255, 0, 0, 0, 255, 0, 0, 0, 255
    };
    static unsigned char const hls_expected[9] = {
        235, 51, 36, 117, 252, 74, 0, 0, 245
    };
    unsigned char input[320 * 3];
    unsigned char expected[320 * 3];
    unsigned char tga[18 + 320 * 4];
    unsigned char png[16384];
    char input_path[1024];
    char output_path[1024];
    char target[1030];
    sixel_encoder_t *encoder;
    sixel_dither_t *dither;
    edge_frame_probe_t *probe;
    edge_loader_options_t options;
    SIXELSTATUS status;
    FILE *file;
    size_t size;
    int p3;
    int width;
    int depth;
    int mask;
    int phase;
    int i;
    int c;
    int ok;

    encoder = NULL;
    probe = NULL;
    file = NULL;
    ok = 0;
    mask = mode == 4;
    width = mode == 3 ? 3 : 320;
    depth = mask ? 4 : 3;
    if (directory == NULL || strlen(directory) > 900) {
        goto end;
    }
    (void)sixel_compat_snprintf(input_path, sizeof(input_path),
        "%s/input.tga", directory);
    (void)sixel_compat_snprintf(output_path, sizeof(output_path),
        "%s/output.png", directory);
    (void)sixel_compat_snprintf(target, sizeof(target),
        "%s%s", mode == 6 ? "" : "png:", output_path);
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'L',
                "builtin:cms_engine=none!")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '.',
                mode == 0 ? "8bit" : "float32")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "greedy:B1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'A', "keep")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 't',
                mode == 3 ? "hls" : "rgb"))) {
        goto end;
    }
    probe = calloc(1, sizeof(*probe));
    if (probe == NULL) {
        goto end;
    }
    for (phase = 0; phase < (mode == 5 ? 3 : 1); ++phase) {
        p3 = mode >= 2 && !(mode == 5 && phase == 1);
        if (!hp_samples(input, expected, p3)) {
            goto end;
        }
        if (mode == 3) {
            memcpy(input, primaries, sizeof(primaries));
            memcpy(expected, hls_expected, sizeof(hls_expected));
        }
        memset(tga, 0, sizeof(tga));
        tga[2] = 2;
        tga[12] = (unsigned char)width;
        tga[13] = (unsigned char)(width >> 8);
        tga[14] = 1;
        tga[16] = (unsigned char)(depth * 8);
        tga[17] = (unsigned char)(0x20 | (mask ? 8 : 0));
        for (i = 0; i < width; ++i) {
            for (c = 0; c < 3; ++c) {
                tga[18 + i * depth + c] = input[i * 3 + 2 - c];
            }
            if (mask) {
                tga[18 + i * depth + 3] = i == 47 ? 0 : 255;
            }
        }
        file = sixel_compat_fopen(input_path, "wb");
        size = (size_t)(18 + width * depth);
        if (file == NULL || fwrite(tga, 1, size, file) != size) {
            goto end;
        }
        i = fclose(file);
        file = NULL;
        if (i != 0 ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                    p3 ? "display-p3" : "gamma")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'o', target)) ||
                SIXEL_FAILED(sixel_encoder_encode(encoder, input_path))) {
            goto end;
        }
        file = sixel_compat_fopen(output_path, "rb");
        if (file == NULL) {
            goto end;
        }
        size = fread(png, 1, sizeof(png), file);
        i = fclose(file);
        file = NULL;
        if (i != 0 || size < 45 || size == sizeof(png) ||
                memcmp(png, "\211PNG\r\n\032\n", 8) != 0 ||
                png[24] != 8 || png[25] != 6 ||
                !png_color_metadata(png, size, p3)) {
            goto end;
        }
        edge_loader_options_init(&options);
        options.cms_engine = SIXEL_CMS_ENGINE_NONE;
        options.use_palette = 0;
        options.set_prefer_float32 = 1;
        options.prefer_float32 = 0;
        memset(probe, 0, sizeof(*probe));
        if (edge_load_buffer_options("high-color PNG", png, size, &options,
                probe, &status) || status != SIXEL_OK ||
                probe->callback_count != 1 || probe->width[0] != width ||
                probe->height[0] != 1 ||
                probe->pixelformat[0] != SIXEL_PIXELFORMAT_RGB888) {
            goto end;
        }
        for (i = 0; i < width; ++i) {
            if ((probe->mask_size[0] && probe->mask[0][i] != 0) !=
                    (mask && i == 47) ||
                    (!(mask && i == 47) && memcmp(probe->rgb[0] + i * 3,
                        expected + i * 3, 3) != 0)) {
                fprintf(stderr, "PNG mode %d phase %d pixel %d differs\n",
                    mode, phase, i);
                goto end;
            }
        }
        dither = encoder->diagnostic_dither;
        if (mode <= 1 && (dither == NULL ||
                (SIXEL_PIXELFORMAT_IS_FLOAT32(dither->pixelformat) !=
                    (mode == 1)))) {
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "high-color PNG %d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    if (file != NULL) {
        fclose(file);
    }
    free(probe);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
