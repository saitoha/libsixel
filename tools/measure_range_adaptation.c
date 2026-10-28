/* Measure encode_frame with memory-buffered SIXEL and decode its last result.
 * This documentation helper accepts only the trusted P6 fixtures prepared by
 * reproduce_range_adaptation.py. Loading, frame setup, decoding and image IO
 * stay outside the clock. Each arm starts with one unmeasured warmup.
 */
#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sixel.h>

struct wire_buffer {
    unsigned char *data;
    size_t size;
    size_t capacity;
};

static void
require_at(int condition, int line)
{
    if (!condition) {
        fprintf(stderr, "range measurement failed at line %d: %s\n", line,
                sixel_helper_get_additional_message());
        exit(1);
    }
}

#define require(ok) require_at((ok), __LINE__)

static int
capture(char *data, int size, void *context)
{
    struct wire_buffer *wire;
    size_t needed;

    wire = context;
    require(size >= 0 && (size_t)size <= SIZE_MAX - wire->size);
    needed = wire->size + (size_t)size;
    if (needed > wire->capacity) {
        require(needed <= SIZE_MAX / 2);
        wire->capacity = needed * 2;
        wire->data = realloc(wire->data, wire->capacity);
        require(wire->data != NULL);
    }
    memcpy(wire->data + wire->size, data, (size_t)size);
    wire->size = needed;
    return size;
}

static double
now(void)
{
    struct timespec ts;

    require(clock_gettime(CLOCK_MONOTONIC, &ts) == 0);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static int
compare_time(const void *a, const void *b)
{
    double x;
    double y;

    x = *(double const *)a;
    y = *(double const *)b;
    return (x > y) - (x < y);
}

int
main(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    sixel_frame_t *frame;
    sixel_output_t *output;
    SIXELSTATUS status;
    struct wire_buffer wire;
    unsigned char *source;
    unsigned char *decoded;
    unsigned char *palette;
    unsigned char *rgb;
    unsigned char *seen;
    FILE *file;
    char option[128];
    int width;
    int height;
    int dw;
    int dh;
    int colors;
    int repeats;
    int run;
    unsigned int value;
    unsigned int unique;
    size_t pixels;
    size_t i;
    double times[101];
    double start;

    if (argc != 8) {
        fprintf(stderr, "usage: %s MODEL INPUT P SPACE STRENGTH "
                "REPEATS OUTPUT\n", argv[0]);
        return 1;
    }
    repeats = atoi(argv[6]);
    require(repeats > 0 && repeats <= 101 && repeats % 2 == 1);
    file = fopen(argv[2], "rb");
    require(file != NULL);
    require(fscanf(file, "P6 %d %d 255", &width, &height) == 2);
    require(fgetc(file) == '\n' && width > 0 && height > 0);
    require((size_t)width <= SIZE_MAX / (size_t)height / 3);
    pixels = (size_t)width * height;
    source = malloc(pixels * 3);
    rgb = malloc(pixels * 3);
    require(source != NULL && rgb != NULL);
    require(fread(source, 3, pixels, file) == pixels);
    require(fclose(file) == 0);
    memset(&wire, 0, sizeof(wire));
    encoder = NULL;
    output = NULL;
    require(SIXEL_SUCCEEDED(sixel_encoder_new(&encoder, NULL)));
    require(SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, '=', "greedy:B1")));
    snprintf(option, sizeof(option), "%s%s", argv[1],
             strcmp(argv[1], "heckbert") == 0 ? "" : ":seed=1");
    require(SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'Q', option)));
    require(SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'p', argv[3])));
    require(SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, 'X', argv[4])));
    snprintf(option, sizeof(option), "auto:strength=%s", argv[5]);
    require(SIXEL_SUCCEEDED(sixel_encoder_setopt(encoder, '5', option)));
    require(SIXEL_SUCCEEDED(sixel_output_new(&output, capture, &wire, NULL)));
    for (run = -1; run < repeats; ++run) {
        wire.size = 0;
        frame = NULL;
        require(SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)));
        require(SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, source,
            width, height, SIXEL_PIXELFORMAT_RGB888, NULL, -1)));
        sixel_frame_set_multiframe(frame, 0);
        start = now();
        status = sixel_encoder_encode_frame(encoder, frame, output);
        if (run >= 0) {
            times[run] = (now() - start) * 1000;
        }
        require(SIXEL_SUCCEEDED(status));
        sixel_frame_unref(frame);
    }
    decoded = palette = NULL;
    require(wire.size <= INT_MAX);
    require(SIXEL_SUCCEEDED(sixel_decode_raw(wire.data, (int)wire.size,
        &decoded, &dw, &dh, &palette, &colors, NULL)));
    require(dw == width && dh == height);
    seen = calloc(1u << 24, 1);
    require(seen != NULL);
    unique = 0;
    for (i = 0; i < pixels; ++i) {
        memcpy(rgb + i * 3, palette + decoded[i] * 3, 3);
        value = ((unsigned int)rgb[i * 3] << 16) |
            ((unsigned int)rgb[i * 3 + 1] << 8) | rgb[i * 3 + 2];
        if (!seen[value]) {
            seen[value] = 1;
            ++unique;
        }
    }
    qsort(times, (size_t)repeats, sizeof(double), compare_time);
    printf("%.6f,%.6f,%zu,%u\n", times[repeats / 2], times[repeats - 1],
           wire.size, unique);
    file = fopen(argv[7], "wb");
    require(file != NULL);
    fprintf(file, "P6\n%d %d\n255\n", width, height);
    require(fwrite(rgb, 3, pixels, file) == pixels);
    require(fclose(file) == 0);
    sixel_encoder_unref(encoder);
    sixel_output_unref(output);
    free(source);
    free(rgb);
    free(decoded);
    free(palette);
    free(seen);
    free(wire.data);
    return 0;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
/* vim: set et sw=4 sts=4: */
