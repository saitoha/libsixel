/* SPDX-License-Identifier: MIT */
/* Check glyph framing separately from its direct-decoded high-color paint. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sixel.h>
#include "src/encoder.h"
#include "src/dither.h"
#include "src/output.h"
#include "src/sixel_decode_pixels.h"

#if defined(TIOCGWINSZ) && HAVE_UNISTD_H && !defined(__EMSCRIPTEN__)
typedef struct hd_capture {
    unsigned char bytes[65536];
    size_t size;
    int fail;
} hd_capture_t;

static int
hd_write(char *data, int size, void *context)
{
    hd_capture_t *capture;

    capture = context;
    if (capture->fail) {
        return -1;
    }
    if (size < 0 || (size_t)size >=
            sizeof(capture->bytes) - capture->size) {
        return -1;
    }
    memcpy(capture->bytes + capture->size, data, (size_t)size);
    capture->size += (size_t)size;
    capture->bytes[capture->size] = 0;
    return size;
}

static int
hd_check(hd_capture_t const *capture, int mode, int ordinary,
         int width, int height, int const *expected)
{
    unsigned char normal[65536];
    unsigned char *pixels;
    unsigned char *palette;
    unsigned char const *body;
    sixel_allocator_t *allocator;
    char prefix[96];
    char raster[48];
    size_t size;
    size_t head;
    size_t tail;
    unsigned int flags;
    int w;
    int h;
    int colors;
    int i;
    int c;
    int ok;

    pixels = palette = NULL;
    allocator = NULL;
    ok = 0;
    tail = mode == 1 ? 1 : 2;
    if (ordinary) {
        strcpy(prefix, "\033P");
    } else {
        snprintf(prefix, sizeof(prefix), "%s%s%s1;0;0;%d;1;3;6;0{%s",
            mode == 1 ? "\233" : "\033[",
            mode == 2 ? "?8800;8801h" : "?8800h",
            mode == 1 ? "\220" : "\033P", mode == 0 ? 4 : 2,
            mode == 2 ? "  @" : " 0");
    }
    head = strlen(prefix);
    if (capture->size <= head + tail ||
            memcmp(capture->bytes, prefix, head) != 0 ||
            memcmp(capture->bytes + capture->size - tail,
                mode == 1 ? "\234" : "\033\\", tail) != 0) {
        goto end;
    }
    body = capture->bytes + head;
    size = capture->size - head - tail;
    if (ordinary) {
        memcpy(normal, capture->bytes, capture->size);
        size = capture->size;
    } else {
        snprintf(raster, sizeof(raster), "\"1;1;%d;%d", width, height);
        if (strncmp((char const *)body, raster, strlen(raster)) != 0 ||
                memchr(body, '\033', size) != NULL ||
                memchr(body, '\220', size) != NULL ||
                memchr(body, '\234', size) != NULL ||
                size + 5 >= sizeof(normal)) {
            goto end;
        }
        /* Only the extracted body is re-enveloped for the paint oracle.
         * The decoder is not a DRCS receiver or a glyph-storage oracle. */
        memcpy(normal, "\033Pq", 3);
        memcpy(normal + 3, body, size);
        memcpy(normal + 3 + size, "\033\\", 2);
        size += 5;
    }
    normal[size] = 0;
    if (mode == 3 && strstr((char *)normal, "#0;1;120;50;100") == NULL) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_decode_direct_with_options(normal,
                (int)size, 0, &pixels, &w, &h, &palette, &colors,
                &flags, allocator)) || w != width || h != height ||
            (mode == 0 && !(flags &
                SIXEL_DECODE_PIXELS_RESULT_PALETTE_REDEFINED))) {
        goto end;
    }
    for (i = 0; i < width * height; ++i) {
        if (pixels[i * 4 + 3] != 255) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            if (pixels[i * 4 + c] !=
                    (expected[i * 3 + c] * 255 + 50) / 100) {
                fprintf(stderr, "DRCS pixel %d channel %d: %u != %d\n",
                    i, c, pixels[i * 4 + c],
                    (expected[i * 3 + c] * 255 + 50) / 100);
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (allocator != NULL) {
        sixel_allocator_free(allocator, pixels);
        sixel_allocator_free(allocator, palette);
    }
    sixel_allocator_unref(allocator);
    return ok;
}

int
test_highcolor_drcs(int mode)
{
    /* Independent linear P3 -> linear sRGB matrix and analytic transfer.
     * Grid RGB101 fields have .0098 minimum tie margin, exceeding a .001
     * conversion budget. Neutral .234 detects byte reconstruction. */
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
    static int const primary[9] = {100,0,0,0,100,0,23,23,23};
    float samples[320 * 3];
    float saved[320 * 3];
    unsigned char bytes[12] = {64,128,192,64,128,192,64,128,192,0,0,0};
    int expected[320 * 3];
    double device[3];
    double value;
    hd_capture_t capture;
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t alpha;
    sixel_writer_controls_t controls;
    sixel_dither_t *dither;
    unsigned char *mask;
    unsigned char chars[40];
    unsigned char glyphs[39];
    void *input;
    SIXELSTATUS status;
    int channel[2];
    int width;
    int height;
    int format;
    int phase;
    int phases;
    int p3;
    int reject;
    int i;
    int c;
    int j;
    int ok;

    encoder = NULL;
    output = NULL;
    frame = NULL;
    mask = NULL;
    channel[0] = channel[1] = -1;
    ok = 0;
    width = mode == 0 ? 320 : mode == 2 ? 5 : 3;
    height = mode == 2 ? 13 : 1;
    phases = mode == 5 || mode == 8 ? 3 :
        mode >= 6 && mode <= 9 ? 2 : 1;
    memset(samples, 0, sizeof(samples));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, hd_write,
                &capture, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '@',
                mode == 2 ? "3:1" : "2:1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '=', "greedy:B1")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'g', NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'A', "keep")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W',
                mode == 0 ? "linear" : "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 't',
                mode == 3 ? "hls" : "rgb")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder,
                mode == 1 ? '8' : '7', NULL))) {
        goto end;
    }
    /* Cached geometry avoids opening the invoking terminal. Four-pixel
     * cells keep the 320-pixel stress case within one 96-glyph set. */
    encoder->cell_width = mode == 0 ? 4 : 2;
    encoder->cell_height = 6;
    if (mode == 2) {
        if (pipe(channel) != 0) {
            goto end;
        }
        encoder->tile_outfd = channel[1];
    }
    for (phase = 0; phase < phases; ++phase) {
        p3 = mode != 4 && !(mode == 5 && phase == 1);
        reject = mode >= 6 && mode <= 9 &&
            phase < (mode == 8 ? 2 : 1);
        encoder->fdrcs = !(mode == 5 && phase == 1);
        encoder->fuse_macro = mode == 8 && reject && phase == 0;
        encoder->macro_number = mode == 8 && reject && phase == 1 ? 7 : -1;
        encoder->output_is_png = mode == 9 && reject;
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma"))) {
            goto end;
        }
        for (i = 0; i < width * height; ++i) {
            for (c = 0; c < 3; ++c) {
                samples[i * 3 + c] = gamma[(i % 3) * 3 + c];
                expected[i * 3 + c] = primary[(i % 3) * 3 + c];
            }
            if (mode == 0) {
                device[0] = ((i % 32) * 8 + 2) / 255.0;
                device[1] = ((i / 32) * 8 + 2) / 255.0;
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
            } else if (mode == 4) {
                expected[i * 3] = 25;
                expected[i * 3 + 1] = 50;
                expected[i * 3 + 2] = 75;
            } else if (mode == 5) {
                samples[i * 3] = 1;
                samples[i * 3 + 1] = samples[i * 3 + 2] = 0;
                expected[i * 3] = p3 ? 92 : 100;
                expected[i * 3 + 1] = p3 ? 20 : 0;
                expected[i * 3 + 2] = p3 ? 14 : 0;
            }
        }
        memcpy(saved, samples, sizeof(saved));
        format = mode == 4 ? SIXEL_PIXELFORMAT_RGB888 : mode == 0 ?
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
            SIXEL_PIXELFORMAT_RGBFLOAT32;
        input = mode == 4 ? (void *)bytes : (void *)samples;
        if (mode == 7 && reject) {
            format = SIXEL_PIXELFORMAT_RGBA8888;
            input = bytes;
        }
        if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame, input,
                    width, height, format, NULL, -1))) {
            goto end;
        }
        if ((mode == 6 || mode == 7) && reject) {
            memset(&alpha, 0, sizeof(alpha));
            alpha.transparent = -1;
            alpha.alpha_zero_is_transparent = 1;
            if (mode == 6) {
                mask = calloc((size_t)width, 1);
                if (mask == NULL) {
                    goto end;
                }
                mask[1] = 1;
                alpha.transparent_mask = mask;
                alpha.transparent_mask_size = (size_t)width;
            }
            iface = sixel_frame_as_interface(frame);
            if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &alpha))) {
                goto end;
            }
            mask = NULL;
        }
        memset(&capture, 0, sizeof(capture));
        if (mode == 10) {
            /* Preserve an asymmetric caller setting on a writer failure;
             * resetting both flags to zero would also violate ownership. */
            sixel_output_set_skip_header(output, 1);
            capture.fail = 1;
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (mode == 10) {
            ok = SIXEL_FAILED(status) && capture.size == 0 &&
                SIXEL_SUCCEEDED(sixel_output_get_writer_controls(output,
                    &controls)) && !controls.skip_dcs_envelope &&
                controls.skip_header;
            goto end;
        } else if (reject) {
            if (status != (mode < 8 ? SIXEL_BAD_INPUT :
                    SIXEL_BAD_ARGUMENT) || capture.size != 0) {
                goto end;
            }
        } else {
            dither = (sixel_dither_t *)encoder->diagnostic_dither;
            if (SIXEL_FAILED(status) || dither == NULL ||
                    (mode == 4 ?
                        dither->pixelformat != SIXEL_PIXELFORMAT_RGB888 :
                        !SIXEL_PIXELFORMAT_IS_FLOAT32(dither->pixelformat)) ||
                    !hd_check(&capture, mode, !encoder->fdrcs,
                        width, height, expected)) {
                goto end;
            }
        }
        if (memcmp(samples, saved, sizeof(saved)) != 0) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    if (mode == 2) {
        encoder->tile_outfd = -1;
        close(channel[1]);
        channel[1] = -1;
        for (i = 0; i < 9; ++i) {
            c = i * 4 + i / 3;
            glyphs[c] = 0xf4;
            glyphs[c + 1] = glyphs[c + 2] = 0x80;
            glyphs[c + 3] = (unsigned char)(0x80 + i);
            if (i % 3 == 2) {
                glyphs[c + 4] = '\n';
            }
        }
        if (read(channel[0], chars, sizeof(chars)) != 39 ||
                memcmp(chars, glyphs, 39) != 0) {
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "high-color DRCS %d: %s\n", mode,
            sixel_helper_get_additional_message());
    }
    if (encoder != NULL) {
        encoder->tile_outfd = -1;
    }
    if (channel[0] >= 0) {
        close(channel[0]);
    }
    if (channel[1] >= 0) {
        close(channel[1]);
    }
    free(mask);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else
int
test_highcolor_drcs(int mode)
{
    (void)mode;
    return 77;
}
#endif
