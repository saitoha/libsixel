/* SPDX-License-Identifier: MIT */
/* Independent wire endpoints, parsed-state atomicity and ICC ownership. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/color-option.h"
#include "src/compat_stub.h"
#include "tests/data/color-numeric/icc-output.h"
#include "src/options-registry.h"
#include "src/encoder.h"
#include "src/decoder.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/colorspace.h"
#include "src/palette-snap-grid.h"
#include "tests/data/color-numeric/vectors.h"
#include "tests/writer/png/color_test_common.h"

static unsigned char icco_bytes[8192];
static size_t icco_size;

static int
icco_write(char *bytes, int size, void *context)
{
    (void)context;
    if (size < 0 || (size_t)size > sizeof(icco_bytes) - icco_size) {
        return -1;
    }
    memcpy(icco_bytes + icco_size, bytes, (size_t)size);
    icco_size += (size_t)size;
    return size;
}

static int
icco_encode(char const *argument, int mode)
{
    sixel_encoder_t *encoder;
    sixel_output_t *output;
    sixel_frame_t *frame;
    unsigned char *decoded;
    float pixels[9];
    float saved[9];
    int width;
    int height;
    int c;
    int pixel;
    int expected;
    int ok;
    SIXELSTATUS status;
    char replacement[4096];
    char const *slash;
    double converted[3];
    double input[3];
    sixel_palette_t *retained;

    retained = NULL;
    encoder = NULL;
    output = NULL;
    frame = NULL;
    decoded = NULL;
    ok = 0;
    icco_size = 0;
    /* Decimal oracle stores P3 primary XYZ and canonical linear sRGB.
     * Transform errors are far below the final 0.005 wire half bin. */
    for (pixel = 0; pixel < 3; ++pixel) {
        for (c = 0; c < 3; ++c) {
            pixels[pixel * 3 + c] = (float)color_vectors[pixel][9 + c];
        }
    }
    memcpy(saved, pixels, sizeof(saved));
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            SIXEL_FAILED(sixel_output_new(&output, icco_write, NULL, NULL)) ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'Q', "kmeans")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'W', "linear")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'X', "linear")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'd', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'a', "off")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'F', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, '_', "none")) ||
            SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', argument))) {
        goto end;
    }
    if (mode == 2 || mode == 3 || mode == 6) {
        if (SIXEL_FAILED(sixel_encoder_setopt(encoder, '_',
                mode == 3 ? "reversible" : "nearest"))) {
            goto end;
        }
    }
    if (mode == 4 && SIXEL_FAILED(sixel_encoder_setopt(encoder, 'I', NULL))) {
        goto end;
    }
    if (mode == 5) {
        if (sixel_encoder_setopt(encoder, 'U', "icc:path=/nonexistent.icc")
                != SIXEL_BAD_ARGUMENT || encoder->output_icc == NULL) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 3, 1,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    status = sixel_encoder_encode_frame(encoder, frame, output);
    if (mode == 6) {
        ok = status == SIXEL_BAD_ARGUMENT && icco_size == 0;
        goto end;
    }
    if (SIXEL_FAILED(status) || memcmp(pixels, saved, sizeof(saved)) != 0 ||
            SIXEL_FAILED(sixel_decode_direct(icco_bytes, (int)icco_size,
                &decoded, &width, &height, NULL)) ||
            width != 3 || height != 1) {
        goto end;
    }
    for (pixel = 0; pixel < 3; ++pixel) {
        for (c = 0; c < 3; ++c) {
            /* The constant B2A1 fixture deliberately is not an inverse.
             * Its stored output table is exactly (13107,26214,39321)/65535. */
            expected = mode == 1 ? (c + 1) * 51 : pixel == c ? 255 : 0;
            if (decoded[pixel * 4 + c] != expected) {
                fprintf(stderr, "wire[%d,%d]=%u expected %d\n", pixel, c,
                    decoded[pixel * 4 + c], expected);
                goto end;
            }
        }
    }
    if (mode == 7) {
        if (encoder->diagnostic_dither == NULL) {
            goto end;
        }
        retained = ((sixel_dither_t *)encoder->diagnostic_dither)->palette;
        retained->vtbl->ref(retained);
        slash = strrchr(argument, '/');
        if (slash == NULL || (size_t)(slash - argument) > 4000u) {
            goto end;
        }
        snprintf(replacement, sizeof(replacement),
            "%.*s/relative-paired-lut.icc",
            (int)(slash - argument), argument);
        if (SIXEL_FAILED(sixel_encoder_set_accumulation_buffer(encoder,
                decoded, 3, 1, SIXEL_PIXELFORMAT_RGBA8888)) ||
                !encoder->accumulation_valid ||
                SIXEL_FAILED(sixel_encoder_setopt(encoder, 'U', replacement)) ||
                encoder->accumulation_valid) {
            goto end;
        }
        /* The retained palette must own its original transform even after
         * the encoder releases its source ICC and selects a different one. */
        for (c = 0; c < 3; ++c) {
            input[c] = color_vectors[0][9 + c];
        }
        if (!sixel_palette_to_output(retained, converted, input,
                SIXEL_COLORSPACE_LINEAR)) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            if (fabs(converted[c] - (c == 0 ? 1.0 : 0.0)) > .0005) {
                goto end;
            }
        }
        icco_size = 0;
        free(decoded);
        decoded = NULL;
        if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
                SIXEL_FAILED(sixel_decode_direct(icco_bytes, (int)icco_size,
                    &decoded, &width, &height, NULL))) {
            goto end;
        }
        for (pixel = 0; pixel < 3; ++pixel) {
            for (c = 0; c < 3; ++c) {
                if (decoded[pixel * 4 + c] != (c + 1) * 51) {
                    goto end;
                }
            }
        }
    }
    ok = 1;
end:
    if (retained != NULL) {
        retained->vtbl->unref(retained);
    }
    if (!ok) {
        fprintf(stderr, "%s\n", sixel_helper_get_additional_message());
    }
    free(decoded);
    sixel_frame_unref(frame);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return ok;
}

static int
icco_parse(char const *argument, int decoder, int reject)
{
    sixel_color_def_t *definition;
    sixel_color_def_t *saved;
    int base;
    SIXELSTATUS status;
    int ok;

    definition = sixel_cdef_name("srgb");
    saved = definition;
    base = 9876;
    status = sixel_color_option_parse(argument, decoder, &base,
        &definition, NULL);
    if (reject) {
        ok = status == SIXEL_BAD_ARGUMENT && base == 9876 &&
            definition == saved;
    } else {
        ok = SIXEL_SUCCEEDED(status) && definition != NULL &&
            base == (decoder ? SIXEL_NAMED_COLOR_COUNT
                             : -SIXEL_NAMED_COLOR_COUNT);
    }
    if (definition != saved) {
        sixel_cdef_free(definition);
    }
    sixel_cdef_free(saved);
    return ok;
}

static int
icco_decoder(char const *argument)
{
    sixel_decoder_t *decoder;
    sixel_color_def_t *saved;
    double xyz[3];
    double rgb[3] = {1.0, 0.0, 0.0};
    int c;
    int ok;

    decoder = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_decoder_new(&decoder, NULL)) ||
            SIXEL_FAILED(sixel_decoder_setopt(decoder, 'T', argument))) {
        goto end;
    }
    saved = decoder->input_icc;
    if (saved == NULL || sixel_decoder_setopt(decoder, 'T', "icc:path=-")
            != SIXEL_BAD_ARGUMENT || saved != decoder->input_icc ||
            !sixel_cdef_to_xyz(saved, rgb, xyz, 1)) {
        goto end;
    }
    /* Stored ICC matrix quantization: one half s15Fixed16 unit per field. */
    for (c = 0; c < 3; ++c) {
        if (fabs(xyz[c] - color_vectors[0][6 + c]) > 1.0 / 65536.0) {
            goto end;
        }
    }
    ok = SIXEL_SUCCEEDED(sixel_decoder_setopt(decoder, 'T', "srgb")) &&
        decoder->input_icc == NULL && decoder->input_colorspace == 0;
end:
    sixel_decoder_unref(decoder);
    return ok;
}

static int
icco_grid(char const *argument, int strategy)
{
    static unsigned int const codes[] = {0, 10 * 10201 + 50 * 101 + 90,
        100 * 10201 + 100 * 101 + 100};
    sixel_color_def_t *definition;
    sixel_snap_grid_t *grid;
    sixel_allocator_t *allocator;
    double actual[3];
    int base;
    int c;
    int i;
    int ok;

    definition = NULL;
    grid = NULL;
    allocator = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_color_option_parse(argument, 0, &base,
            &definition, NULL)) || SIXEL_FAILED(sixel_allocator_new(&allocator,
                NULL, NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_snap_grid_new(&grid, strategy,
                SIXEL_COLORSPACE_LINEAR, definition, allocator))) {
        goto end;
    }
    sixel_cdef_free(definition);
    definition = NULL;
    for (i = 0; i < 3; ++i) {
        if (SIXEL_FAILED(sixel_snap_grid_target(grid, codes[i], actual))) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            /* Stored ICC is exact; remaining error is double matrix math. */
            if (!isfinite(actual[c]) ||
                    fabs(actual[c] - icco_black_expected[i][c]) > 2e-9) {
                fprintf(stderr, "grid[%d,%d]=%.17g expected %.17g\n", i, c,
                    actual[c], icco_black_expected[i][c]);
                goto end;
            }
        }
    }
    ok = 1;
end:
    sixel_cdef_free(definition);
    sixel_snap_grid_destroy(grid);
    sixel_allocator_unref(allocator);
    return ok;
}

int
test_icc_output(int argc, char **argv)
{
    int mode;

    if (argc != 4) {
        return 1;
    }
    mode = atoi(argv[2]);
    if (strcmp(argv[1], "encode") == 0) {
        return !icco_encode(argv[3], mode);
    }
    if (strcmp(argv[1], "parse") == 0) {
        return !icco_parse(argv[3], mode & 1, mode > 1);
    }
    if (strcmp(argv[1], "decoder") == 0) {
        return !icco_decoder(argv[3]);
    }
    if (strcmp(argv[1], "grid") == 0) {
        return !icco_grid(argv[3], mode);
    }
    if (strcmp(argv[1], "png") == 0) {
        return png_color_check(1, mode == 0 ? 0 : mode == 1 ? 1 : 2,
            mode < 2 ? 0 : mode);
    }
    if (strcmp(argv[1], "snapshot") == 0) {
        return mode ? png_snapshot_direct_check(1, 0)
                    : png_snapshot_check(1, 0);
    }
    return 1;
}
