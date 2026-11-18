#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sixel.h>

#ifndef LEGACY_VERSION
#define LEGACY_VERSION "1.8.7-r2"
#endif

typedef struct {
    unsigned char bytes[1024];
    int length;
} capture_t;

enum {
    fixture_width = 9,
    fixture_height = 6,
    fixture_pixels = fixture_width * fixture_height
};

enum {
    probe_pass,
    probe_fail,
    probe_known_regression
};

typedef struct {
    char const *name;
    int format;
    int bits_per_pixel;
    unsigned char source[2][5];
    unsigned char normalized[2][3];
} pixel_fixture_t;

/* The left and right samples alternate in both x and y. For a packed
 * format each source entry holds a full row, including deliberately
 * nonzero and distinct padding after pixel 9. The normalized entries
 * are literal RGB samples or literal unpacked indices. */
static pixel_fixture_t const fixtures[] = {
    {"rgb555", SIXEL_PIXELFORMAT_RGB555, 16,
        {{0x09, 0x5a}, {0x68, 0x8d}}, {{16, 80, 208}, {208, 32, 104}}},
    {"rgb565", SIXEL_PIXELFORMAT_RGB565, 16,
        {{0x12, 0x9a}, {0xd1, 0x0d}}, {{16, 80, 208}, {208, 32, 104}}},
    {"rgb888", SIXEL_PIXELFORMAT_RGB888, 24,
        {{16, 80, 208}, {208, 32, 104}}, {{16, 80, 208}, {208, 32, 104}}},
    {"bgr555", SIXEL_PIXELFORMAT_BGR555, 16,
        {{0x69, 0x42}, {0x34, 0x9a}}, {{16, 80, 208}, {208, 32, 104}}},
    {"bgr565", SIXEL_PIXELFORMAT_BGR565, 16,
        {{0xd2, 0x82}, {0x69, 0x1a}}, {{16, 80, 208}, {208, 32, 104}}},
    {"bgr888", SIXEL_PIXELFORMAT_BGR888, 24,
        {{208, 80, 16}, {104, 32, 208}}, {{16, 80, 208}, {208, 32, 104}}},
    {"argb8888", SIXEL_PIXELFORMAT_ARGB8888, 32,
        {{255, 16, 80, 208}, {255, 208, 32, 104}},
        {{16, 80, 208}, {208, 32, 104}}},
    {"rgba8888", SIXEL_PIXELFORMAT_RGBA8888, 32,
        {{16, 80, 208, 255}, {208, 32, 104, 255}},
        {{16, 80, 208}, {208, 32, 104}}},
    {"abgr8888", SIXEL_PIXELFORMAT_ABGR8888, 32,
        {{255, 208, 80, 16}, {255, 104, 32, 208}},
        {{16, 80, 208}, {208, 32, 104}}},
    {"bgra8888", SIXEL_PIXELFORMAT_BGRA8888, 32,
        {{208, 80, 16, 255}, {104, 32, 208, 255}},
        {{16, 80, 208}, {208, 32, 104}}},
    {"g1", SIXEL_PIXELFORMAT_G1, 1,
        {{0x55, 0x7f}, {0xaa, 0xd5}}, {{0}, {1}}},
    {"g2", SIXEL_PIXELFORMAT_G2, 2,
        {{0x66, 0x66, 0x6f}, {0x99, 0x99, 0x95}}, {{1}, {2}}},
    {"g4", SIXEL_PIXELFORMAT_G4, 4,
        {{0x5a, 0x5a, 0x5a, 0x5a, 0x5f}, {0xa5, 0xa5, 0xa5, 0xa5, 0xa3}},
        {{5}, {10}}},
    {"g8", SIXEL_PIXELFORMAT_G8, 8,
        {{80}, {208}}, {{80, 80, 80}, {208, 208, 208}}},
    {"ag88", SIXEL_PIXELFORMAT_AG88, 16,
        {{231, 80}, {255, 208}}, {{80, 80, 80}, {208, 208, 208}}},
    {"ga88", SIXEL_PIXELFORMAT_GA88, 16,
        {{80, 231}, {208, 255}}, {{80, 80, 80}, {208, 208, 208}}},
    {"pal1", SIXEL_PIXELFORMAT_PAL1, 1,
        {{0x55, 0x7f}, {0xaa, 0xd5}}, {{0}, {1}}},
    {"pal2", SIXEL_PIXELFORMAT_PAL2, 2,
        {{0x66, 0x66, 0x6f}, {0x99, 0x99, 0x95}}, {{1}, {2}}},
    {"pal4", SIXEL_PIXELFORMAT_PAL4, 4,
        {{0x12, 0x12, 0x12, 0x12, 0x1f}, {0x21, 0x21, 0x21, 0x21, 0x23}},
        {{1}, {2}}},
    {"pal8", SIXEL_PIXELFORMAT_PAL8, 8,
        {{1}, {2}}, {{1}, {2}}}
};

static unsigned char const fixture_palette[] = {
    51, 102, 204,
    204, 51, 102,
    102, 204, 51
};


static int
palette_size(pixel_fixture_t const *fixture)
{
    if ((fixture->format & SIXEL_FORMATTYPE_PALETTE) == 0) {
        return -1;
    }
    return fixture->bits_per_pixel == 1 ? 2 : 3;
}


static size_t
make_source(pixel_fixture_t const *fixture, unsigned char *pixels)
{
    size_t row_bytes;
    size_t depth;
    size_t y;
    size_t x;

    row_bytes = ((size_t)fixture->bits_per_pixel * fixture_width + 7U) / 8U;
    depth = (size_t)fixture->bits_per_pixel / 8U;
    for (y = 0; y < fixture_height; ++y) {
        if (depth == 0) {
            memcpy(pixels + y * row_bytes, fixture->source[y % 2U], row_bytes);
            continue;
        }
        for (x = 0; x < fixture_width; ++x) {
            memcpy(pixels + y * row_bytes + x * depth,
                   fixture->source[(x + y) % 2U], depth);
        }
    }
    return row_bytes * fixture_height;
}


static int
check_frame(pixel_fixture_t const *fixture)
{
    unsigned char source[fixture_pixels * 4];
    unsigned char *pixels = NULL;
    unsigned char *palette = NULL;
    sixel_frame_t *frame = NULL;
    size_t size;
    size_t colors_size;
    int colors;
    int result = 1;
    SIXELSTATUS status;

    size = make_source(fixture, source);
    colors = palette_size(fixture);
    colors_size = colors < 0 ? 0U : (size_t)colors * 3U;
    pixels = malloc(size);
    if (pixels == NULL) {
        goto end;
    }
    memcpy(pixels, source, size);
    if (colors_size > 0) {
        palette = malloc(colors_size);
        if (palette == NULL) {
            goto end;
        }
        memcpy(palette, fixture_palette, colors_size);
    }
    status = sixel_frame_new(&frame, NULL);
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_frame_init(frame, pixels, fixture_width, fixture_height,
                              fixture->format, palette, colors);
    if (status != SIXEL_OK) {
        goto end;
    }

    /* The public constructor takes ownership and the getters expose both
     * original buffers with their original metadata, including padding. */
    if (sixel_frame_get_pixels(frame) == pixels &&
            sixel_frame_get_palette(frame) == palette &&
            sixel_frame_get_width(frame) == fixture_width &&
            sixel_frame_get_height(frame) == fixture_height &&
            sixel_frame_get_pixelformat(frame) == fixture->format &&
            sixel_frame_get_ncolors(frame) == colors &&
            sixel_frame_get_transparent(frame) == -1 &&
            memcmp(pixels, source, size) == 0 &&
            (colors_size == 0 ||
             memcmp(palette, fixture_palette, colors_size) == 0)) {
        result = 0;
    }
    pixels = NULL;
    palette = NULL;

end:
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    free(pixels);
    free(palette);
    return result;
}


static int
check_fixture_normalization(pixel_fixture_t const *fixture)
{
    unsigned char source[fixture_pixels * 4];
    unsigned char actual[fixture_pixels * 3];
    int format = -1;
    int expected_format;
    size_t depth;
    size_t i;
    SIXELSTATUS status;

    make_source(fixture, source);
    if (palette_size(fixture) > 0) {
        expected_format = SIXEL_PIXELFORMAT_PAL8;
    } else if (fixture->bits_per_pixel < 8) {
        expected_format = SIXEL_PIXELFORMAT_G8;
    } else {
        expected_format = SIXEL_PIXELFORMAT_RGB888;
    }
    depth = expected_format == SIXEL_PIXELFORMAT_RGB888 ? 3U : 1U;
    memset(actual, 0xff, sizeof(actual));
    status = sixel_helper_normalize_pixelformat(actual, &format, source,
        fixture->format, fixture_width, fixture_height);
    if (status != SIXEL_OK || format != expected_format ||
            sixel_helper_compute_depth(fixture->format) !=
                (fixture->bits_per_pixel + 7) / 8) {
        fprintf(stderr, "status=0x%x, format=0x%x\n",
                (unsigned int)status, (unsigned int)format);
        return 1;
    }
    for (i = 0; i < fixture_pixels; ++i) {
        if (memcmp(actual + i * depth, fixture->normalized[i % 2U], depth)
                != 0) {
            fprintf(stderr, "unexpected normalized bytes at pixel %zu\n", i);
            return 1;
        }
    }
    return 0;
}


static int
check_dither(pixel_fixture_t const *fixture)
{
    unsigned char source[fixture_pixels * 4];
    sixel_dither_t *dither = NULL;
    unsigned char const *palette;
    SIXELSTATUS status;
    int result = 1;

    make_source(fixture, source);
    status = sixel_dither_new(&dither, 2, NULL);
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_dither_initialize(dither, source,
        fixture_width, fixture_height, fixture->format,
        SIXEL_LARGE_NORM, SIXEL_REP_AVERAGE_COLORS, SIXEL_QUALITY_FULL);
    if (status != SIXEL_OK ||
            sixel_dither_get_num_of_palette_colors(dither) != 2) {
        goto end;
    }
    palette = sixel_dither_get_palette(dither);
    if (palette != NULL &&
            ((memcmp(palette, fixture->normalized[0], 3) == 0 &&
              memcmp(palette + 3, fixture->normalized[1], 3) == 0) ||
             (memcmp(palette, fixture->normalized[1], 3) == 0 &&
              memcmp(palette + 3, fixture->normalized[0], 3) == 0))) {
        result = 0;
    }

end:
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    if (result != 0) {
        fprintf(stderr, "unexpected initialized palette, status=0x%x\n",
                (unsigned int)status);
    }
    return result;
}


static unsigned int
decoded_rgb_channel(unsigned int sample)
{
    unsigned int percent;

    /* SIXEL RGB registers store integral percentages. */
    percent = (sample * 100U + 127U) / 255U;
    return (percent * 255U + 50U) / 100U;
}


static void
expected_rgb(pixel_fixture_t const *fixture, size_t parity,
             int scale_gray, unsigned int *channels)
{
    unsigned char const *sample = fixture->normalized[parity];
    size_t channel;
    unsigned int gray;

    if (palette_size(fixture) > 0) {
        sample = fixture_palette + (size_t)sample[0] * 3U;
    }
    if (fixture->bits_per_pixel < 8 && palette_size(fixture) < 0) {
        gray = sample[0];
        if (scale_gray) {
            gray = gray * 255U /
                ((1U << (unsigned int)fixture->bits_per_pixel) - 1U);
        }
        gray = decoded_rgb_channel(gray);
        for (channel = 0; channel < 3; ++channel) {
            channels[channel] = gray;
        }
        return;
    }
    for (channel = 0; channel < 3; ++channel) {
        channels[channel] = decoded_rgb_channel(sample[channel]);
    }
}


static size_t
first_wrong_pixel(pixel_fixture_t const *fixture,
                  unsigned char const *indices, unsigned char const *palette,
                  int colors, int scale_gray)
{
    unsigned char const *rgb;
    unsigned int expected[3];
    size_t i;

    for (i = 0; i < fixture_pixels; ++i) {
        if ((int)indices[i] >= colors) {
            return i;
        }
        rgb = palette + (size_t)indices[i] * 3U;
        expected_rgb(fixture, i % 2U, scale_gray, expected);
        if (rgb[0] != expected[0] || rgb[1] != expected[1] ||
                rgb[2] != expected[2]) {
            return i;
        }
    }
    return fixture_pixels;
}


static int
check_encode_bytes(pixel_fixture_t const *fixture, char const *path,
                  int historical)
{
    unsigned char source[fixture_pixels * 4];
    unsigned char expected_source[fixture_pixels * 4];
    unsigned char entries[sizeof(fixture_palette)];
    unsigned char wire[8192];
    unsigned char *indices = NULL;
    unsigned char *decoded = NULL;
    sixel_encoder_t *encoder = NULL;
    char const *message;
    struct stat wire_file;
    size_t size;
    size_t i;
    ssize_t wire_size;
    int fd = -1;
    int width = 0;
    int height = 0;
    int colors = 0;
    int result = probe_fail;
    int palette_colors = palette_size(fixture);
    int gray_alpha = fixture->format == SIXEL_PIXELFORMAT_AG88 ||
                     fixture->format == SIXEL_PIXELFORMAT_GA88;
    int packed_gray = fixture->bits_per_pixel < 8 && palette_colors < 0;
    unsigned int scaled[2][3];
    unsigned int unscaled[2][3];
    SIXELSTATUS status;

    size = make_source(fixture, source);
    memcpy(expected_source, source, size);
    memcpy(entries, fixture_palette, sizeof(entries));
    status = sixel_encoder_new(&encoder, NULL);
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTFILE, path);
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none");
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_PALETTE_TYPE, "rgb");
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_encoder_encode_bytes(encoder, source,
        fixture_width, fixture_height, fixture->format,
        palette_colors > 0 ? entries : NULL, palette_colors);
    if (memcmp(source, expected_source, size) != 0 ||
            memcmp(entries, fixture_palette, sizeof(entries)) != 0) {
        fprintf(stderr, "encode_bytes changed caller-owned input\n");
        goto end;
    }
    message = sixel_helper_get_additional_message();
    sixel_encoder_unref(encoder);
    encoder = NULL;
    if (status != SIXEL_OK || (historical && gray_alpha)) {
        if (status != SIXEL_LOGIC_ERROR ||
                stat(path, &wire_file) != 0 || wire_file.st_size != 0) {
            goto end;
        }
        if (historical && gray_alpha) {
            printf("PASS legacy encode-bytes %s rejects gray with alpha\n",
                   fixture->name);
            result = probe_pass;
        } else if (!historical &&
                   fixture->format == SIXEL_PIXELFORMAT_BGR888 &&
                   message != NULL &&
                   strcmp(message, "sixel_frame_convert_to_rgb888: "
                                   "invalid pixelformat.") == 0) {
            printf("KNOWN REGRESSION current encode-bytes bgr888 "
                   "(0x%x, no bytes, %s)\n", (unsigned int)status, message);
            result = probe_known_regression;
        }
        goto end;
    }
    fd = open(path, O_RDONLY);
    if (fd == -1) {
        goto end;
    }
    wire_size = read(fd, wire, sizeof(wire));
    if (wire_size <= 0 || wire_size == (ssize_t)sizeof(wire)) {
        goto end;
    }
    status = sixel_decode_raw(wire, (int)wire_size, &indices, &width,
                              &height, &decoded, &colors, NULL);
    if (status != SIXEL_OK || width != fixture_width ||
            height != fixture_height || indices == NULL || decoded == NULL) {
        goto end;
    }
    i = first_wrong_pixel(fixture, indices, decoded, colors, 1);
    if (i == fixture_pixels) {
        if (gray_alpha || packed_gray ||
                fixture->format == SIXEL_PIXELFORMAT_BGR888) {
            printf("PASS %s encode-bytes %s RGB pixels\n",
                   historical ? "legacy" : "current", fixture->name);
        }
        result = probe_pass;
    } else if (!historical && packed_gray &&
               first_wrong_pixel(fixture, indices, decoded, colors, 0)
                   == fixture_pixels) {
        expected_rgb(fixture, 0, 1, scaled[0]);
        expected_rgb(fixture, 1, 1, scaled[1]);
        expected_rgb(fixture, 0, 0, unscaled[0]);
        expected_rgb(fixture, 1, 0, unscaled[1]);
        printf("KNOWN REGRESSION current encode-bytes %s: 8-bit grayscale "
               "%u/%u instead of %u/%u across all %d pixels\n",
               fixture->name, unscaled[0][0], unscaled[1][0],
               scaled[0][0], scaled[1][0], fixture_pixels);
        result = probe_known_regression;
    } else {
        fprintf(stderr, "unexpected decoded RGB at pixel %zu\n", i);
    }

end:
    if (result == probe_fail) {
        fprintf(stderr, "encode_bytes status=0x%x, decoded %dx%d/%d\n",
                (unsigned int)status, width, height, colors);
    }
    if (fd != -1) {
        close(fd);
    }
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    free(indices);
    free(decoded);
    return result;
}


static int
check_fixtures(char const *case_name, char const *path, int historical)
{
    pixel_fixture_t const *fixture;
    int result = probe_pass;
    int observation;
    size_t i;

    for (i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); ++i) {
        fixture = &fixtures[i];
        if (strcmp(case_name, "frame") == 0) {
            observation = check_frame(fixture);
        } else if (strcmp(case_name, "normalize") == 0) {
            observation = check_fixture_normalization(fixture);
        } else if (strcmp(case_name, "dither") == 0) {
            /* r2 treats the normalized output as RGB888. Packed grayscale
             * and indexed normalization instead returns one byte per pixel. */
            if (fixture->bits_per_pixel < 8 || palette_size(fixture) > 0) {
                continue;
            }
            observation = check_dither(fixture);
        } else {
            observation = check_encode_bytes(fixture, path, historical);
        }
        if (observation == probe_fail) {
            fprintf(stderr, "FAIL %s %s\n", case_name, fixture->name);
            result = probe_fail;
        } else if (observation == probe_known_regression &&
                   result == probe_pass) {
            result = probe_known_regression;
        }
    }
    return result;
}


static int
loaded_library_matches(char const *path)
{
    Dl_info info;
    struct stat loaded;
    struct stat expected;

    /* A system copy with the same SONAME must not satisfy the probe. */
    if (dladdr((void *)sixel_helper_compute_depth, &info) == 0) {
        fprintf(stderr, "could not identify the loaded libsixel\n");
        return 0;
    }
    if (stat(info.dli_fname, &loaded) != 0 || stat(path, &expected) != 0 ||
            loaded.st_dev != expected.st_dev ||
            loaded.st_ino != expected.st_ino) {
        fprintf(stderr, "loaded %s, expected %s\n", info.dli_fname, path);
        return 0;
    }
    return 1;
}


static int
check_normalization(unsigned char const *source,
                    int format,
                    int depth,
                    unsigned char const *expected)
{
    unsigned char pixels[6] = {0};
    int result_format = -1;
    SIXELSTATUS status;

    status = sixel_helper_normalize_pixelformat(pixels, &result_format,
                                               source, format, 2, 1);
    if (status != SIXEL_OK || result_format != SIXEL_PIXELFORMAT_RGB888 ||
            sixel_helper_compute_depth(format) != depth ||
            memcmp(pixels, expected, sizeof(pixels)) != 0) {
        fprintf(stderr, "status=0x%x, format=0x%x, RGB=%u,%u,%u / %u,%u,%u\n",
                (unsigned int)status, (unsigned int)result_format,
                (unsigned int)pixels[0], (unsigned int)pixels[1],
                (unsigned int)pixels[2], (unsigned int)pixels[3],
                (unsigned int)pixels[4], (unsigned int)pixels[5]);
        return 1;
    }
    return 0;
}


static int
check_rgb565(void)
{
    /* The legacy helper reads the high byte first and shifts 5/6/5 bits
     * into the high bits of the corresponding output channels. */
    static unsigned char const packed[] = {0x8d, 0x45, 0x1a, 0x7c};
    static unsigned char const expected[] = {136, 168, 40, 24, 76, 224};

    return check_normalization(packed, SIXEL_PIXELFORMAT_RGB565, 2, expected);
}


static int
check_bgr888(void)
{
    static unsigned char const packed[] = {27, 62, 199, 249, 7, 104};
    static unsigned char const expected[] = {199, 62, 27, 104, 7, 249};

    return check_normalization(packed, SIXEL_PIXELFORMAT_BGR888, 3, expected);
}


static int
capture_sixel(char *bytes, int length, void *context)
{
    capture_t *capture = context;

    if (length < 0 || (size_t)length > sizeof(capture->bytes) -
                                     (size_t)capture->length) {
        return -1;
    }
    memcpy(capture->bytes + capture->length, bytes, (size_t)length);
    capture->length += length;
    return length;
}


static int
check_pal8(int partial_band, int historical)
{
    /* Palette samples are exact in both byte RGB and SIXEL's 0..100
     * palette coordinates. The legacy encoder emits a leading line
     * advance for a partial first band. */
    unsigned char source[] = {
        2, 0, 1,
        2, 1, 0,
        1, 2, 0,
        1, 0, 2,
        0, 1, 2,
        0, 2, 1
    };
    unsigned char entries[] = {51, 102, 204, 204, 51, 102, 102, 204, 51};
    static unsigned char const expected[][3] = {
        {102, 204, 51}, {51, 102, 204}, {204, 51, 102},
        {102, 204, 51}, {204, 51, 102}, {51, 102, 204},
        {204, 51, 102}, {102, 204, 51}, {51, 102, 204},
        {204, 51, 102}, {51, 102, 204}, {102, 204, 51},
        {51, 102, 204}, {204, 51, 102}, {102, 204, 51},
        {51, 102, 204}, {102, 204, 51}, {204, 51, 102}
    };
    capture_t capture = {{0}, 0};
    sixel_dither_t *dither = NULL;
    sixel_output_t *output = NULL;
    unsigned char *indices = NULL;
    unsigned char *palette = NULL;
    SIXELSTATUS status;
    int width = 0;
    int height = 0;
    int colors = 0;
    int source_height = partial_band ? 2 : 6;
    int leading_rows = partial_band && historical ? 6 : 0;
    size_t i;
    int result = 1;

    status = sixel_dither_new(&dither, 3, NULL);
    if (status != SIXEL_OK) {
        goto end;
    }
    status = sixel_output_new(&output, capture_sixel, &capture, NULL);
    if (status != SIXEL_OK) {
        goto end;
    }
    sixel_dither_set_palette(dither, entries);
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_PAL8);
    sixel_dither_set_optimize_palette(dither, 0);
    sixel_output_set_palette_type(output, SIXEL_PALETTETYPE_RGB);
    status = sixel_encode(source, 3, source_height, 1, dither, output);
    if (status != SIXEL_OK || capture.length == 0) {
        goto end;
    }
    status = sixel_decode_raw(capture.bytes, capture.length, &indices,
                              &width, &height, &palette, &colors, NULL);
    if (status != SIXEL_OK || width != 3 ||
            height != source_height + leading_rows || colors < 3 ||
            indices == NULL || palette == NULL) {
        goto end;
    }

    /* Palette register numbering may change. Compare the resolved colors
     * with the literal source-order oracle. */
    for (i = 0; i < (size_t)leading_rows * 3U; ++i) {
        if (indices[i] != 255U) {
            fprintf(stderr, "unexpected paint in the blank leading band\n");
            goto end;
        }
    }
    for (i = (size_t)leading_rows * 3U; i < (size_t)height * 3U; ++i) {
        if ((int)indices[i] >= colors ||
                memcmp(palette + (size_t)indices[i] * 3U,
                       expected[i - (size_t)leading_rows * 3U], 3) != 0) {
            fprintf(stderr, "PAL8 RGB differs at pixel %zu (register %u)\n",
                    i, (unsigned int)indices[i]);
            goto end;
        }
    }
    if (partial_band) {
        printf("PASS %s partial-band layout (%d decoded leading rows)\n",
               historical ? "legacy" : "current", leading_rows);
    }
    result = 0;

end:
    if (result != 0) {
        fprintf(stderr, "PAL8 status=0x%x, SIXEL bytes=%d, decoded %dx%d/%d\n",
                (unsigned int)status, capture.length, width, height, colors);
    }
    free(indices);
    free(palette);
    if (output != NULL) {
        sixel_output_unref(output);
    }
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    return result;
}


int
main(int argc, char **argv)
{
    int historical;

    if (argc != 5 || strcmp(LIBSIXEL_VERSION, LEGACY_VERSION) != 0) {
        fprintf(stderr, "expected a client built against v%s\n",
                LEGACY_VERSION);
        return 1;
    }
    if (!loaded_library_matches(argv[1])) {
        return 1;
    }
    historical = strcmp(argv[4], "current") != 0;
    if (strcmp(argv[2], "rgb565") == 0) {
        return check_rgb565();
    }
    if (strcmp(argv[2], "bgr888") == 0) {
        return check_bgr888();
    }
    if (strcmp(argv[2], "pal8") == 0) {
        return check_pal8(0, 0);
    }
    if (strcmp(argv[2], "pal8-partial-band") == 0) {
        return check_pal8(1, historical);
    }
    if (strcmp(argv[2], "frame") == 0 ||
            strcmp(argv[2], "normalize") == 0 ||
            strcmp(argv[2], "dither") == 0 ||
            strcmp(argv[2], "encode-bytes") == 0) {
        return check_fixtures(argv[2], argv[3], historical);
    }
    fprintf(stderr, "unknown legacy client case: %s\n", argv[2]);
    return 1;
}
