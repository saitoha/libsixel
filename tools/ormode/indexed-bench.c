/* Measure serialization from one decoded, immutable palette-index raster. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sixel.h>

struct sink {
    FILE *file;
    size_t bytes;
};

static int
write_data(char *data, int size, void *opaque)
{
    struct sink *sink;

    sink = (struct sink *)opaque;
    sink->bytes += (size_t)size;
    if (sink->file != NULL &&
        fwrite(data, 1, (size_t)size, sink->file) != (size_t)size) {
        return -1;
    }
    return size;
}

static double
now(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int
main(int argc, char **argv)
{
    FILE *file;
    unsigned char *stream;
    unsigned char *pixels;
    unsigned char *copy;
    unsigned char *palette;
    sixel_dither_t *dither;
    sixel_output_t *output;
    SIXELSTATUS status;
    struct sink sink;
    long length;
    int width;
    int height;
    int ncolors;
    int iteration;
    int order;
    int mode;
    int runs;
    double begin;
    double elapsed;
    char name[4096];

    if (argc != 4) {
        return 2;
    }
    runs = atoi(argv[3]);
    file = fopen(argv[1], "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
        return 3;
    }
    length = ftell(file);
    if (length <= 0 || fseek(file, 0, SEEK_SET) != 0) {
        return 4;
    }
    stream = malloc((size_t)length);
    if (stream == NULL ||
        fread(stream, 1, (size_t)length, file) != (size_t)length) {
        return 5;
    }
    fclose(file);
    pixels = NULL;
    palette = NULL;
    status = sixel_decode_raw(stream, (int)length, &pixels,
                             &width, &height, &palette, &ncolors, NULL);
    if (SIXEL_FAILED(status)) {
        return 6;
    }
    copy = malloc((size_t)width * (size_t)height);
    if (copy == NULL) {
        return 7;
    }
    /* Context setup and input copying stay outside the measured interval.
     * The first pair writes evidence streams. Two more pairs warm caches.
     * Subsequent pairs alternate mode order and use the same count sink. */
    for (iteration = -3; iteration < runs; iteration++) {
        for (order = 0; order < 2; order++) {
            mode = (iteration + 4 + order) % 2;
            memcpy(copy, pixels, (size_t)width * (size_t)height);
            status = sixel_dither_new(&dither, ncolors, NULL);
            if (SIXEL_FAILED(status)) {
                return 8;
            }
            sixel_dither_set_palette(dither, palette);
            sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_PAL8);
            sink.file = NULL;
            sink.bytes = 0;
            if (iteration == -3) {
                snprintf(name, sizeof(name), "%s.%s.six", argv[2],
                         mode ? "or" : "normal");
                sink.file = fopen(name, "wb");
                if (sink.file == NULL) {
                    return 9;
                }
            }
            status = sixel_output_new(&output, write_data, &sink, NULL);
            if (SIXEL_FAILED(status)) {
                return 10;
            }
            sixel_output_set_ormode(output, mode);
            sixel_output_set_encode_policy(output, SIXEL_ENCODEPOLICY_SIZE);
            begin = now();
            status = sixel_encode(copy, width, height, 1, dither, output);
            elapsed = now() - begin;
            if (SIXEL_FAILED(status)) {
                fprintf(stderr, "encode failed: %x\n", status);
                return 11;
            }
            if (sink.file != NULL) {
                fclose(sink.file);
            }
            if (iteration >= 0) {
                printf("%d,%d,%.9f,%zu\n", iteration, mode,
                       elapsed, sink.bytes);
            }
            sixel_output_unref(output);
            sixel_dither_unref(dither);
        }
    }
    free(copy);
    free(palette);
    free(pixels);
    free(stream);
    return 0;
}
