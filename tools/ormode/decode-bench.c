/* Decode fixed normal/OR streams, excluding input I/O and output encoding. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <sixel.h>

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
    unsigned char *streams[2];
    unsigned char *pixels;
    unsigned char *expected;
    unsigned char *palette;
    unsigned char *rgba;
    long lengths[2];
    size_t bytes;
    size_t index;
    int width;
    int height;
    int expected_width;
    int expected_height;
    int mode;
    int order;
    int iteration;
    int runs;
    int indexed;
    int ncolors;
    double begin;
    double elapsed;
    SIXELSTATUS status;

    if (argc != 6 || setenv("SIXEL_THREADS", argv[4], 1) != 0) {
        return 2;
    }
    runs = atoi(argv[3]);
    indexed = strcmp(argv[5], "indexed") == 0;
    for (mode = 0; mode < 2; mode++) {
        file = fopen(argv[mode + 1], "rb");
        if (file == NULL || fseek(file, 0, SEEK_END) != 0) {
            return 3;
        }
        lengths[mode] = ftell(file);
        if (lengths[mode] <= 0 || lengths[mode] > INT_MAX ||
            fseek(file, 0, SEEK_SET) != 0) {
            return 4;
        }
        streams[mode] = malloc((size_t)lengths[mode]);
        if (streams[mode] == NULL ||
            fread(streams[mode], 1, (size_t)lengths[mode], file) !=
                (size_t)lengths[mode]) {
            return 5;
        }
        fclose(file);
    }
    expected = NULL;
    expected_width = 0;
    expected_height = 0;
    /* Alternate order after two warmup pairs. Fresh output allocations are
     * included; comparison and freeing are outside the timed decode call. */
    for (iteration = -2; iteration < runs; iteration++) {
        for (order = 0; order < 2; order++) {
            mode = (iteration + 2 + order) % 2;
            pixels = NULL;
            palette = NULL;
            begin = now();
            if (indexed) {
                status = sixel_decode_raw(streams[mode],
                                          (int)lengths[mode], &pixels,
                                          &width, &height, &palette,
                                          &ncolors, NULL);
            } else {
                status = sixel_decode_direct(streams[mode],
                                             (int)lengths[mode], &pixels,
                                             &width, &height, NULL);
            }
            elapsed = now() - begin;
            if (SIXEL_FAILED(status) || width < 1 || height < 1) {
                return 6;
            }
            bytes = (size_t)width * (size_t)height * 4U;
            rgba = pixels;
            if (indexed) {
                rgba = malloc(bytes);
                if (rgba == NULL) {
                    return 7;
                }
                for (index = 0; index < bytes / 4U; index++) {
                    if (pixels[index] >= ncolors) {
                        return 9;
                    }
                    memcpy(rgba + index * 4U,
                           palette + pixels[index] * 3U, 3U);
                    rgba[index * 4U + 3U] = 255;
                }
            }
            if (expected == NULL) {
                expected = malloc(bytes);
                rgba = pixels;
            if (indexed) {
                rgba = malloc(bytes);
                if (rgba == NULL) {
                    return 7;
                }
                for (index = 0; index < bytes / 4U; index++) {
                    if (pixels[index] >= ncolors) {
                        return 9;
                    }
                    memcpy(rgba + index * 4U,
                           palette + pixels[index] * 3U, 3U);
                    rgba[index * 4U + 3U] = 255;
                }
            }
            if (expected == NULL) {
                    return 7;
                }
                memcpy(expected, rgba, bytes);
                expected_width = width;
                expected_height = height;
            }
            if (width != expected_width || height != expected_height ||
                memcmp(expected, rgba, bytes) != 0) {
                fprintf(stderr, "decoded pixels differ at mode %d\n", mode);
                return 8;
            }
            if (iteration >= 0) {
                printf("%d,%d,%.9f,%d,%d\n", iteration, mode, elapsed,
                       width, height);
            }
            if (indexed) {
                free(rgba);
                free(palette);
            }
            free(pixels);
        }
    }
    free(expected);
    free(streams[0]);
    free(streams[1]);
    return 0;
}
