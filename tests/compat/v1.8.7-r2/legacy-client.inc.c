#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <sixel.h>

typedef struct {
    unsigned char bytes[1024];
    int length;
} capture_t;


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
check_pal8(void)
{
    /* One complete sixel band avoids the leading line advance that the
     * legacy encoder emitted for a partial first band. Palette samples
     * are exact in both byte RGB and SIXEL's 0..100 palette coordinates. */
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
    status = sixel_encode(source, 3, 6, 1, dither, output);
    if (status != SIXEL_OK || capture.length == 0) {
        goto end;
    }
    status = sixel_decode_raw(capture.bytes, capture.length, &indices,
                              &width, &height, &palette, &colors, NULL);
    if (status != SIXEL_OK || width != 3 || height != 6 || colors < 3 ||
            indices == NULL || palette == NULL) {
        goto end;
    }

    /* Palette register numbering may change. Compare the resolved colors
     * with the literal source-order oracle. */
    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        if ((int)indices[i] >= colors ||
                memcmp(palette + (size_t)indices[i] * 3U,
                       expected[i], sizeof(expected[i])) != 0) {
            fprintf(stderr, "PAL8 RGB differs at pixel %zu (register %u)\n",
                    i, (unsigned int)indices[i]);
            goto end;
        }
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
    if (argc != 3 || strcmp(LIBSIXEL_VERSION, "1.8.7-r2") != 0) {
        fprintf(stderr, "expected a client built against v1.8.7-r2\n");
        return 1;
    }
    if (!loaded_library_matches(argv[1])) {
        return 1;
    }
    if (strcmp(argv[2], "rgb565") == 0) {
        return check_rgb565();
    }
    if (strcmp(argv[2], "bgr888") == 0) {
        return check_bgr888();
    }
    if (strcmp(argv[2], "pal8") == 0) {
        return check_pal8();
    }
    fprintf(stderr, "unknown legacy client case: %s\n", argv[2]);
    return 1;
}
