/* SPDX-License-Identifier: MIT */
/* Exercise the real fd-backed macro callback, then inspect its inner paint. */
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
#include "src/sixel_decode_pixels.h"

static int
hm_nibble(unsigned char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static int
hm_check(char const *path, int mode, int phase, int width,
         int const *expected)
{
    unsigned char wire[65536];
    unsigned char inner[32768];
    unsigned char *pixels;
    unsigned char *palette;
    sixel_allocator_t *allocator;
    FILE *file;
    char prefix[40];
    char suffix[40];
    size_t size;
    size_t pos;
    size_t count;
    unsigned int flags;
    int id;
    int w;
    int h;
    int ncolors;
    int i;
    int c;
    int hi;
    int lo;
    int alpha;
    int ok;

    allocator = NULL;
    pixels = palette = NULL;
    file = sixel_compat_fopen(path, "rb");
    ok = 0;
    if (file == NULL) {
        perror("high-color macro capture");
        goto end;
    }
    size = fread(wire, 1, sizeof(wire) - 1, file);
    if (ferror(file) || size == sizeof(wire) - 1) {
        goto end;
    }
    wire[size] = 0;
    id = mode == 0 || mode == 6 || mode == 7 ? 7 :
        mode == 8 ? phase % 2 : 0;
    (void)sixel_compat_snprintf(suffix, sizeof(suffix), "\033[%d*z", id);
    pos = 0;
    if (mode == 8) {
        /* The file sink has no terminal geometry: scroll homes the cursor. */
        if (size < 3 || memcmp(wire, "\033[H", 3) != 0) {
            goto end;
        }
        pos = 3;
        if (phase >= 2) {
            ok = size - pos == strlen(suffix) &&
                memcmp(wire + pos, suffix, strlen(suffix)) == 0;
            goto end;
        }
    }
    (void)sixel_compat_snprintf(prefix, sizeof(prefix),
        "\033P%d;0;1!z", id);
    count = strlen(prefix);
    if (size - pos < count || memcmp(wire + pos, prefix, count) != 0) {
        goto end;
    }
    pos += count;
    count = 0;
    while (pos + 1 < size && wire[pos] != '\033') {
        hi = hm_nibble(wire[pos]);
        lo = hm_nibble(wire[pos + 1]);
        if (hi < 0 || lo < 0 || count >= sizeof(inner) - 1) {
            goto end;
        }
        inner[count++] = (unsigned char)(hi * 16 + lo);
        pos += 2;
    }
    inner[count] = 0;
    if (pos + 2 > size || memcmp(wire + pos, "\033\\", 2) != 0) {
        goto end;
    }
    pos += 2;
    if (id == 7) {
        suffix[0] = 0;
    }
    if (size - pos != strlen(suffix) ||
            memcmp(wire + pos, suffix, strlen(suffix)) != 0 || count < 4) {
        goto end;
    }
    if (mode == 2) {
        if (inner[0] != 0x90 || inner[count - 1] != 0x9c) {
            goto end;
        }
    } else if (memcmp(inner, "\033P", 2) != 0 ||
            memcmp(inner + count - 2, "\033\\", 2) != 0) {
        goto end;
    }
    if (mode == 3 && strstr((char *)inner,
            "#0;1;120;50;100") == NULL) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_decode_direct_with_options(inner,
                (int)count, 0, &pixels, &w, &h, &palette, &ncolors,
                &flags, allocator)) || w != width || h != 1 ||
            (mode == 0 && !(flags &
                SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED))) {
        goto end;
    }
    for (i = 0; i < width; ++i) {
        alpha = (mode == 4 || mode == 5) && i == 1 ? 0 : 255;
        if (pixels[i * 4 + 3] != alpha) {
            goto end;
        }
        for (c = 0; c < 3 && alpha; ++c) {
            if (pixels[i * 4 + c] !=
                    (expected[i * 3 + c] * 255 + 50) / 100) {
                fprintf(stderr, "macro pixel %d channel %d: %u != %d\n",
                    i, c, pixels[i * 4 + c],
                    (expected[i * 3 + c] * 255 + 50) / 100);
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (file != NULL) {
        fclose(file);
    }
    if (allocator != NULL) {
        sixel_allocator_free(allocator, pixels);
        sixel_allocator_free(allocator, palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

int
test_highcolor_macro(int mode, char const *directory)
{
    /* Independent linear P3 -> linear sRGB matrix; target grid RGB101
     * values are integer-rational expectations. Nearest tie margin .0098
     * field units exceeds a conservative .001 conversion-error budget.
     * Gamma-space P3 red/green references use analytic sRGB transfer;
     * neutral .234 has .1 field margin and detects byte reconstruction. */
    static double const matrix[3][3] = {
        {1.2249401762806, -.22494017628056, 0},
        {-.042056954709688, 1.0420569547097, 0},
        {-.019637554590334, -.078636045550632, 1.098273600140966}
    };
    static float const gamma[9] = {
        1.0930663624352f, -.22674197356975f, -.15013458093712f,
        -.51160498258534f, 1.0182656579378f, -.31067462129058f,
        .234f, .234f, .234f
    };
    static int const primary_fields[9] = {100,0,0,0,100,0,23,23,23};
    unsigned char bytes[12] = {255,0,0,255,0,255,0,0,60,60,60,255};
    float samples[320 * 3];
    float saved[320 * 3];
    int expected[320 * 3];
    double device[3];
    double value;
    char path[1024];
    unsigned char *mask;
    void *input;
    sixel_encoder_t *encoder;
    sixel_dither_t *dither;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t alpha;
    int mode_float;
    int width;
    int phase;
    int phases;
    int format;
    int p3;
    int i;
    int c;
    int j;
    int ok;

    encoder = NULL;
    frame = NULL;
    mask = NULL;
    ok = 0;
    memset(samples, 0, sizeof(samples));
    width = mode == 0 ? 320 : 3;
    phases = mode == 7 ? 3 : mode == 8 ? 4 : 1;
    if (directory == NULL || strlen(directory) > 900) {
        goto end;
    }
    (void)sixel_compat_snprintf(path, sizeof(path), "%s/macro.six", directory);
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "greedy:B1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'g', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'A', "keep")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                mode == 0 ? "linear" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                mode == 0 || mode == 6 || mode == 7 ? 'n' : 'u',
                mode == 0 || mode == 6 || mode == 7 ? "7" : NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 't',
                mode == 3 ? "hls" : "rgb")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                mode == 2 ? '8' : '7', NULL))) {
        goto end;
    }
    for (phase = 0; phase < phases; ++phase) {
        p3 = mode != 6 && !(mode == 7 && phase == 1);
        format = mode == 0 ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
            mode == 5 ? SIXEL_PIXELFORMAT_RGBA8888 :
            mode == 6 ? SIXEL_PIXELFORMAT_RGB888 :
            SIXEL_PIXELFORMAT_RGBFLOAT32;
        input = mode == 5 || mode == 6 ? (void *)bytes : (void *)samples;
        memcpy(samples, gamma, sizeof(gamma));
        memcpy(expected, primary_fields, sizeof(primary_fields));
        if (mode == 0) {
            for (i = 0; i < width; ++i) {
                device[0] = (i % 32 * 8 + 2) / 255.0;
                device[1] = (i / 32 * 8 + 2) / 255.0;
                device[2] = .234;
                for (c = 0; c < 3; ++c) {
                    expected[i * 3 + c] = (int)(device[c] * 100 + .5);
                    device[c] = device[c] <= .04045 ? device[c] / 12.92 :
                        pow((device[c] + .055) / 1.055, 2.4);
                }
                for (c = 0; c < 3; ++c) {
                    value = 0;
                    for (j = 0; j < 3; ++j) {
                        value += matrix[c][j] * device[j];
                    }
                    samples[i * 3 + c] = (float)value;
                }
            }
        } else if (mode == 5) {
            expected[0] = 92; expected[1] = 20; expected[2] = 14;
            expected[6] = expected[7] = expected[8] = 24;
        } else if (mode == 6 || mode == 7 || mode == 8) {
            for (i = 0; i < 9; ++i) {
                bytes[i] = (unsigned char)(64 * (i % 3 + 1));
                samples[i] = mode == 8 ?
                    (phase % 2 ? .344f : .234f) : i % 3 == 0 ? 1.f : 0.f;
                expected[i] = mode == 6 ? 25 * (i % 3 + 1) :
                    mode == 8 ? (phase % 2 ? 34 : 23) :
                    i % 3 == 0 ? (p3 ? 92 : 100) :
                    p3 ? (i % 3 == 1 ? 20 : 14) : 0;
            }
        }
        memcpy(saved, samples, sizeof(samples));
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                    p3 ? "display-p3" : "gamma")) ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'o', path)) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, input,
                    width, 1, format, NULL, -1))) {
            goto end;
        }
        if (mode == 4 || mode == 5) {
            if (mode == 4) {
                mask = calloc(3, 1);
                if (mask == NULL) {
                    goto end;
                }
                mask[1] = 1;
            }
            memset(&alpha, 0, sizeof(alpha));
            alpha.transparent = -1;
            /* Packed storage alone does not request alpha interpretation. */
            alpha.alpha_zero_is_transparent = 1;
            alpha.transparent_mask = mask;
            alpha.transparent_mask_size = mask != NULL ? 3 : 0;
            iface = sixel_frame_as_interface(frame);
            if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &alpha))) {
                goto end;
            }
            mask = NULL;
        }
        if (mode == 8) {
            sixel_frame_set_multiframe(frame, 1);
            sixel_frame_set_frame_no(frame, phase % 2);
            sixel_frame_set_loop_count(frame, phase / 2);
        }
        /* NULL output is essential: a raw supplied callback bypasses hex. */
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, NULL)) ||
                memcmp(samples, saved, sizeof(samples)) != 0) {
            goto end;
        }
        /* RMS holds an exclusive writer lock until the fd is closed.
         * Switching the sink closes it while preserving encoder state for
         * the reuse and animation replay phases below. */
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'o', "/dev/null")) ||
                !hm_check(path, mode, phase, width, expected) ||
                encoder->diagnostic_dither == NULL) {
            goto end;
        }
        dither = encoder->diagnostic_dither;
        mode_float = SIXEL_PIXELFORMAT_IS_FLOAT32(dither->pixelformat);
        if (mode_float != (mode != 6)) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "high-color macro %d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    free(mask);
    sixel_frame_unref(frame);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
