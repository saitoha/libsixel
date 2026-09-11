/* SPDX-License-Identifier: MIT */
/* Measure complete loader calls, excluding the untimed pixel export. */
#define _POSIX_C_SOURCE 200809L
#include <sixel.h>
#include <6cells.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct observation {
    char const *dump;
    int frames;
    int format;
    int width;
    int height;
};

static SIXELSTATUS
observe(sixel_frame_t *frame, void *context)
{
    struct observation *obs;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t transparency;
    FILE *file;
    size_t count;
    size_t i;
    size_t x;
    size_t y;
    size_t stride;
    int bits;
    int index;
    int channel;
    float sample[4];
    SIXELSTATUS status;

    obs = context;
    ++obs->frames;
    obs->format = sixel_frame_get_pixelformat(frame);
    obs->width = sixel_frame_get_width(frame);
    obs->height = sixel_frame_get_height(frame);
    if (obs->dump == NULL) {
        return SIXEL_OK;
    }
    iface = sixel_frame_as_interface(frame);
    status = iface->vtbl->get_pixels(iface, &view);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = iface->vtbl->get_transparency(iface, &transparency);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    file = fopen(obs->dump, "wb");
    if (file == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    count = (size_t)view.width * (size_t)view.height;
    for (i = 0; i < count; ++i) {
        index = -1;
        sample[3] = 1.0f;
        if (view.pixelformat == SIXEL_PIXELFORMAT_RGB888) {
            for (channel = 0; channel < 3; ++channel) {
                sample[channel] = view.pixels[i * 3 + channel] / 255.0f;
            }
        } else if (view.pixelformat == SIXEL_PIXELFORMAT_RGBA8888) {
            for (channel = 0; channel < 4; ++channel) {
                sample[channel] = view.pixels[i * 4 + channel] / 255.0f;
            }
        } else if (view.pixelformat == SIXEL_PIXELFORMAT_RGBFLOAT32 ||
                   view.pixelformat == SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) {
            for (channel = 0; channel < 3; ++channel) {
                sample[channel] = view.pixels_float32[i * 3 + channel];
            }
        } else if (view.pixelformat >= SIXEL_PIXELFORMAT_PAL1 &&
                   view.pixelformat <= SIXEL_PIXELFORMAT_PAL8) {
            bits = 1 << (view.pixelformat - SIXEL_PIXELFORMAT_PAL1);
            stride = ((size_t)view.width * bits + 7) / 8;
            x = i % (size_t)view.width;
            y = i / (size_t)view.width;
            index = (view.pixels[y * stride + x * bits / 8] >>
                     (8 - bits - (x * bits % 8))) & ((1 << bits) - 1);
            for (channel = 0; channel < 3; ++channel) {
                sample[channel] = view.palette[index * 3 + channel] / 255.0f;
            }
        } else {
            fclose(file);
            return SIXEL_BAD_ARGUMENT;
        }
        if ((transparency.transparent_mask != NULL &&
             i < transparency.transparent_mask_size &&
             transparency.transparent_mask[i] != 0) ||
            (index >= 0 && index == transparency.transparent)) {
            sample[3] = 0.0f;
        }
        if (fwrite(sample, sizeof(float), 4, file) != 4) {
            fclose(file);
            return SIXEL_BAD_ARGUMENT;
        }
    }
    return fclose(file) == 0 ? SIXEL_OK : SIXEL_BAD_ARGUMENT;
}

static double
seconds(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec * 1e-9;
}

int
main(int argc, char **argv)
{
    sixel_loader_t *loader;
    struct observation obs;
    SIXELSTATUS status;
    unsigned char background[3];
    int palette;
    int require_static;
    int repeats;
    int i;
    double start;
    double elapsed;

    if (argc != 7) {
        fprintf(stderr, "usage: %s ORDER INPUT REPEATS DUMP BG PALETTE\n",
                argv[0]);
        return 2;
    }
    repeats = atoi(argv[3]);
    if (repeats < 1) {
        return 2;
    }
    palette = atoi(argv[6]);
    require_static = 1;
    background[0] = background[1] = background[2] = 128;
    obs.dump = argv[4];
    obs.frames = 0;
    status = sixel_loader_new(&loader, NULL);
    if (SIXEL_FAILED(status)) {
        return 1;
    }
#define SET(option, value) do { \
    status = sixel_loader_setopt(loader, option, value); \
    if (SIXEL_FAILED(status)) { goto failure; } \
} while (0)
    SET(SIXEL_LOADER_OPTION_LOADER_ORDER, argv[1]);
    SET(SIXEL_LOADER_OPTION_CONTEXT, &obs);
    SET(SIXEL_LOADER_OPTION_REQUIRE_STATIC, &require_static);
    SET(SIXEL_LOADER_OPTION_USE_PALETTE, &palette);
    if (atoi(argv[5]) != 0) {
        SET(SIXEL_LOADER_OPTION_BGCOLOR, background);
    }
    status = sixel_loader_load_file(loader, argv[2], observe);
    if (SIXEL_FAILED(status) || obs.frames != 1) {
        goto failure;
    }
    obs.dump = NULL;
    /* Warm the allocator and file cache before each measured batch. */
    for (i = 0; i < 2; ++i) {
        status = sixel_loader_load_file(loader, argv[2], observe);
        if (SIXEL_FAILED(status)) {
            goto failure;
        }
    }
    start = seconds();
    for (i = 0; i < repeats; ++i) {
        status = sixel_loader_load_file(loader, argv[2], observe);
        if (SIXEL_FAILED(status)) {
            goto failure;
        }
    }
    elapsed = seconds() - start;
    printf("{\"ms\":%.9f,\"format\":%d,\"width\":%d,\"height\":%d}\n",
           elapsed * 1000.0 / repeats, obs.format, obs.width, obs.height);
    sixel_loader_unref(loader);
    return 0;
failure:
    fprintf(stderr, "loader status=%x: %s\n", status,
            sixel_helper_get_additional_message());
    sixel_loader_unref(loader);
    return 1;
}
