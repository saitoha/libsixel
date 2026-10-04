/* SPDX-License-Identifier: MIT */
/* Native glyph bodies retain working floats and exact final wire values. */
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
#include "src/palette.h"

#if defined(TIOCGWINSZ) && HAVE_UNISTD_H && !defined(__EMSCRIPTEN__)
typedef struct drcs_native_capture {
    char bytes[16384];
    size_t length;
} drcs_native_capture_t;

static int
drcs_native_write(char *data, int size, void *context)
{
    drcs_native_capture_t *capture;

    capture = context;
    if (size < 0 || (size_t)size >=
            sizeof(capture->bytes) - capture->length) {
        return -1;
    }
    memcpy(capture->bytes + capture->length, data, (size_t)size);
    capture->length += (size_t)size;
    capture->bytes[capture->length] = '\0';
    return size;
}

static int
drcs_native_definition(drcs_native_capture_t const *capture, int index,
                       int red, int green, int blue)
{
    char expected[64];
    char const *found;

    snprintf(expected, sizeof(expected), "#%d;2;%d;%d;%d",
        index, red, green, blue);
    found = strstr(capture->bytes, expected);
    return found != NULL && (found[strlen(expected)] < '0' ||
        found[strlen(expected)] > '9');
}

static int
drcs_native_body(drcs_native_capture_t const *capture, int c1, int mmv,
                 int width, int height, int const *expected)
{
    char prefix[96];
    char normal[16384];
    char raster[48];
    char const *body;
    size_t prefix_size;
    size_t body_size;
    size_t tail_size;
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char *palette;
    int w;
    int h;
    int count;
    int n;
    int ok;

    allocator = NULL;
    pixels = NULL;
    palette = NULL;
    ok = 0;
    snprintf(prefix, sizeof(prefix), "%s%s%s1;0;0;2;1;3;6;0{%s",
        c1 ? "\233" : "\033[", mmv == 3 ? "?8800;8801h" : "?8800h",
        c1 ? "\220" : "\033P", mmv == 3 ? "  @" : " 0");
    prefix_size = strlen(prefix);
    tail_size = c1 ? 1 : 2;
    if (capture->length <= prefix_size + tail_size ||
            strncmp(capture->bytes, prefix, prefix_size) != 0 ||
            memcmp(capture->bytes + capture->length - tail_size,
                c1 ? "\234" : "\033\\", tail_size) != 0) {
        return 0;
    }
    body = capture->bytes + prefix_size;
    body_size = capture->length - prefix_size - tail_size;
    snprintf(raster, sizeof(raster), "\"1;1;%d;%d", width, height);
    if (strncmp(body, raster, strlen(raster)) != 0 ||
            memchr(body, '\033', body_size) != NULL ||
            memchr(body, '\220', body_size) != NULL ||
            body_size + 5 > sizeof(normal)) {
        return 0;
    }
    /* The envelope is checked literally above. Only the bitmap body is
     * wrapped for the existing independent index oracle; no glyph parser
     * or receiver color interpretation is inferred from this decoder. */
    memcpy(normal, "\033Pq", 3);
    memcpy(normal + 3, body, body_size);
    memcpy(normal + 3 + body_size, "\033\\", 2);
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
                NULL, NULL)) ||
            SIXEL_FAILED(sixel_decode_raw((unsigned char *)normal,
                (int)body_size + 5, &pixels, &w, &h, &palette, &count,
                allocator)) || w != width || h != height) {
        goto end;
    }
    for (n = 0; n < w * h; ++n) {
        if (pixels[n] != expected[n]) {
            fprintf(stderr, "DRCS pixel %d = %u, expected %d\n",
                n, pixels[n], expected[n]);
            goto end;
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
drcs_native_contract(int scenario)
{
    static struct { int flag; char const *value; } const options[] = {
        { '=', "1" }, { '@', "2:1" }, { 'Q', "heckbert" },
        { 'F', "none" }, { '_', "none" }, { '~', "none" },
        { 'd', "none" }, { 'E', "size" }, { 'X', "linear" },
        { 'W', "linear" }, { '7', NULL }, { 'g', NULL },
        { '4', "full-frame" }, { '5', "none" }
    };
    static float const gray[3] = { .1f, .1001f, .5f };
    static float const fixed[4] = { 0.f, .0908417112f, .40197778f, 1.f };
    static int const wire_gray[3] = { 35, 35, 74 };
    float pixels[5 * 13 * 3];
    float saved[5 * 13 * 3];
    unsigned char bytes[5 * 13 * 3];
    int expected[5 * 13];
    int mapping[4];
    int channel[2];
    unsigned char chars[40];
    unsigned char chars_expected[39];
    unsigned char *mask;
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    sixel_frame_transparency_t transparency;
    sixel_frame_interface_t *iface;
    drcs_native_capture_t capture;
    SIXELSTATUS status;
    size_t n;
    unsigned int i;
    int step;
    int steps;
    int width;
    int height;
    int colors;
    int c;
    int p3;
    int centroid;
    int byte;
    int builtin;
    int match;
    int ok;
    double target;

    encoder = NULL;
    frame = NULL;
    output = NULL;
    mask = NULL;
    channel[0] = channel[1] = -1;
    ok = 0;
    byte = scenario == 5;
    builtin = scenario == 4;
    centroid = scenario != 3 && scenario != 12 && !builtin && !byte;
    width = centroid ? 2 : 5;
    height = centroid ? 1 : 13;
    colors = centroid ? 1 : (builtin || byte) ? 4 : 3;
    steps = scenario == 11 ? 3 : 1;
    for (n = 0; n < sizeof(pixels) / sizeof(pixels[0]); ++n) {
        c = (int)(n / 3) % 3;
        pixels[n] = builtin ? (c == 0 ? .1f : c == 1 ? .4f : .8f) : gray[c];
        bytes[n] = (unsigned char)(c * 127);
    }
    if (centroid) {
        pixels[0] = 2.f;
        pixels[3] = -.25f;
        pixels[1] = pixels[2] = pixels[4] = pixels[5] = .1f;
    }
    memcpy(saved, pixels, sizeof(saved));
    memset(&capture, 0, sizeof(capture));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, drcs_native_write,
                &capture, NULL))) {
        goto end;
    }
    for (n = 0; n < sizeof(options) / sizeof(options[0]); ++n) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder,
                options[n].flag, options[n].value))) {
            goto end;
        }
    }
    /* Use the same cached geometry the production ioctl fills. These unit
     * tests must never open the invoking user's /dev/tty. The CLI companion
     * uses a fresh controlling PTY and actually exercises the ioctl. */
    encoder->cell_width = 2;
    encoder->cell_height = 6;
    if (!builtin && !byte && scenario != 7 &&
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p',
                centroid ? "1" : "3"))) {
        goto end;
    }
    if ((builtin || byte) && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            'b', "gray2"))) {
        goto end;
    }
    if (byte && (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "gamma")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "gamma")))) {
        goto end;
    }
    if ((scenario == 2 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '8', NULL))) ||
            (scenario == 3 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '@', "3:1"))) ||
            (scenario == 7 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'e', NULL))) ||
            (scenario == 8 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'O', NULL))) ||
            (scenario == 9 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '+', "1,1"))) ||
            (scenario == 10 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                'n', "7"))) ||
            (scenario == 12 && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                '=', "4")))) {
        goto end;
    }
    if (scenario == 3) {
        if (pipe(channel) != 0) {
            goto end;
        }
        encoder->tile_outfd = channel[1];
    }
    for (step = 0; step < steps; ++step) {
        capture.length = 0;
        capture.bytes[0] = '\0';
        p3 = scenario != 0 && !byte && !(scenario == 11 && step == 1);
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U',
                p3 ? "display-p3" : "gamma")) ||
                SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                SIXEL_FAILED(sixel_frame_init_borrowed(frame,
                    byte ? (void *)bytes : (void *)pixels, width, height,
                    byte ? SIXEL_PIXELFORMAT_RGB888 :
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
            goto end;
        }
        if (scenario == 6) {
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "2"))) {
                goto end;
            }
            mask = calloc(2, 1);
            if (mask == NULL) {
                goto end;
            }
            mask[1] = 1;
            memset(&transparency, 0, sizeof(transparency));
            transparency.transparent = -1;
            transparency.alpha_zero_is_transparent = 1;
            transparency.transparent_mask = mask;
            transparency.transparent_mask_size = 2;
            iface = sixel_frame_as_interface(frame);
            if (SIXEL_FAILED(iface->vtbl->set_transparency(iface,
                    &transparency))) {
                goto end;
            }
            mask = NULL;
            status = sixel_encoder_encode_frame(encoder, frame, output);
            if (status != SIXEL_BAD_INPUT || capture.length != 0) {
                goto end;
            }
            sixel_frame_unref(frame);
            frame = NULL;
            if (SIXEL_FAILED(sixel_encoder_setopt(encoder, 'p', "1")) ||
                    SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
                    SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels,
                        2, 1, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
                goto end;
            }
        }
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (scenario >= 7 && scenario <= 10) {
            ok = status == SIXEL_BAD_ARGUMENT && capture.length == 0;
            goto end;
        }
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        dither = (sixel_dither_t *)encoder->diagnostic_dither;
        if (dither == NULL || dither->use_rgb101_palette != !byte ||
                memcmp(pixels, saved, sizeof(saved)) != 0 ||
                dither->ncolors != colors ||
                (byte && dither->pixelformat != SIXEL_PIXELFORMAT_RGB888) ||
                (scenario == 12 && dither->pipeline_last_encode_threads < 2)) {
            goto end;
        }
        if (!byte) {
            if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
                    dither->palette, &view)) || view.entries == NULL ||
                    view.entry_count != (unsigned int)colors) {
                goto end;
            }
            for (c = 0; c < colors; ++c) {
                match = 0;
                for (i = 0; i < view.entry_count; ++i) {
                    for (n = 0; n < 3; ++n) {
                        target = centroid ? (n == 0 ? .875 : .1) :
                            builtin ? fixed[c] : gray[c];
                        if (!isfinite(view.entries[i * 3 + n]) ||
                                fabs(view.entries[i * 3 + n] - target) >
                                    5e-7) {
                            break;
                        }
                    }
                    if (n == 3) {
                        match = 1;
                        break;
                    }
                }
                if (!match) {
                    goto end;
                }
                mapping[c] = (int)i;
                if (centroid) {
                    /* Independent Decimal oracle; margins exceed .0753
                     * RGB101 units, not a production transform round trip. */
                    if (!drcs_native_definition(&capture, (int)i,
                            p3 ? 87 : 94, p3 ? 39 : 35, p3 ? 37 : 35)) {
                        goto end;
                    }
                } else {
                    match = builtin ? (c * 100 + 1) / 3 : wire_gray[c];
                    if (!drcs_native_definition(&capture, (int)i,
                            match, match, match)) {
                        goto end;
                    }
                }
            }
        } else {
            mapping[0] = 0;
            mapping[1] = 1;
            mapping[2] = 3;
        }
        for (n = 0; n < (size_t)(width * height); ++n) {
            expected[n] = centroid ? mapping[0] :
                builtin ? (int)(n % 3) + 1 : mapping[n % 3];
        }
        if (!drcs_native_body(&capture, scenario == 2,
                scenario == 3 ? 3 : 2, width, height, expected)) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
    }
    if (scenario == 3) {
        encoder->tile_outfd = -1;
        close(channel[1]);
        channel[1] = -1;
        for (n = 0; n < 9; ++n) {
            c = (int)n * 4 + (int)n / 3;
            chars_expected[c] = 0xf4;
            chars_expected[c + 1] = 0x80;
            chars_expected[c + 2] = 0x80;
            chars_expected[c + 3] = (unsigned char)(0x80 + n);
            if (n % 3 == 2) {
                chars_expected[c + 4] = '\n';
            }
        }
        if (read(channel[0], chars, sizeof(chars)) != 39 ||
                memcmp(chars, chars_expected, 39) != 0) {
            goto end;
        }
    }
    ok = 1;
end:
    if (!ok) {
        fprintf(stderr, "native DRCS case %d: %s\n", scenario,
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
drcs_native_contract(int scenario)
{
    (void)scenario;
    return 77;
}
#endif
