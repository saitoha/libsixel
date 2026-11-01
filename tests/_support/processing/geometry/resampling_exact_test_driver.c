/*
 * SPDX-License-Identifier: MIT
 *
 * Load binary P6 fixtures through the builtin loader and compare the scalar
 * byte scaler with an independently calculated expected PPM.  Shell TAP
 * wrappers select one method and geometry per test so failures stay local.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "compat_stub.h"

typedef struct resampling_exact_image {
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    int width;
    int height;
    int pixelformat;
    int stride;
    int frame_count;
} resampling_exact_image_t;

typedef struct resampling_exact_method {
    char const *name;
    int value;
} resampling_exact_method_t;

static SIXELSTATUS
resampling_exact_capture(sixel_frame_t *frame, void *private_data)
{
    resampling_exact_image_t *image;
    unsigned char const *pixels;
    size_t pixel_count;
    size_t pixel_bytes;

    image = (resampling_exact_image_t *)private_data;
    if (image == NULL || frame == NULL || image->frame_count != 0) {
        return SIXEL_BAD_INPUT;
    }
    image->width = sixel_frame_get_width(frame);
    image->height = sixel_frame_get_height(frame);
    image->pixelformat = sixel_frame_get_pixelformat(frame);
    image->stride = image->width * 3;
    pixels = sixel_frame_get_pixels(frame);
    if (image->width <= 0 || image->height <= 0 || pixels == NULL ||
        image->pixelformat != SIXEL_PIXELFORMAT_RGB888) {
        return SIXEL_BAD_INPUT;
    }
    pixel_count = (size_t)image->width * (size_t)image->height;
    if (pixel_count / (size_t)image->width != (size_t)image->height ||
        pixel_count > SIZE_MAX / 3u) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_bytes = pixel_count * 3u;
    image->pixels = (unsigned char *)sixel_allocator_malloc(
        image->allocator, pixel_bytes);
    if (image->pixels == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(image->pixels, pixels, pixel_bytes);
    image->frame_count = 1;
    return SIXEL_OK;
}

static SIXELSTATUS
resampling_exact_read_file(char const *path,
                           sixel_allocator_t *allocator,
                           unsigned char **bytes,
                           size_t *size)
{
    FILE *stream;
    long file_size;
    unsigned char *buffer;
    size_t read_size;

    stream = NULL;
    file_size = 0L;
    buffer = NULL;
    read_size = 0u;
    *bytes = NULL;
    *size = 0u;

    stream = sixel_compat_fopen(path, "rb");
    if (stream == NULL || fseek(stream, 0L, SEEK_END) != 0) {
        goto fail;
    }
    file_size = ftell(stream);
    if (file_size <= 0L || fseek(stream, 0L, SEEK_SET) != 0) {
        goto fail;
    }
    buffer = (unsigned char *)sixel_allocator_malloc(
        allocator, (size_t)file_size);
    if (buffer == NULL) {
        fclose(stream);
        return SIXEL_BAD_ALLOCATION;
    }
    read_size = fread(buffer, 1u, (size_t)file_size, stream);
    if (read_size != (size_t)file_size || fclose(stream) != 0) {
        sixel_allocator_free(allocator, buffer);
        return SIXEL_LIBC_ERROR;
    }
    *bytes = buffer;
    *size = read_size;
    return SIXEL_OK;

fail:
    if (stream != NULL) {
        fclose(stream);
    }
    return SIXEL_LIBC_ERROR;
}

static SIXELSTATUS
resampling_exact_decode(char const *path,
                        sixel_allocator_t *allocator,
                        resampling_exact_image_t *image)
{
    SIXELSTATUS status;
    sixel_decode_options_t options;
    sixel_decode_result_t result;
    unsigned char *bytes;
    size_t size;

    status = SIXEL_FALSE;
    bytes = NULL;
    size = 0u;
    memset(image, 0, sizeof(*image));
    memset(&options, 0, sizeof(options));
    memset(&result, 0, sizeof(result));
    image->allocator = allocator;
    options.preferred_pixelformat = SIXEL_PIXELFORMAT_RGB888;

    status = resampling_exact_read_file(path, allocator, &bytes, &size);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_decode_pixels(bytes,
                                 size,
                                 &options,
                                 &result,
                                 allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (result.width <= 0 || result.height <= 0 ||
        result.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        result.stride != result.width * 3 || result.pixels == NULL) {
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    image->pixels = result.pixels;
    image->width = result.width;
    image->height = result.height;
    image->pixelformat = result.pixelformat;
    image->stride = result.stride;
    image->frame_count = 1;
    result.pixels = NULL;
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, bytes);
    sixel_allocator_free(allocator, result.pixels);
    return status;
}

static SIXELSTATUS
resampling_exact_load(char const *path,
                      sixel_allocator_t *allocator,
                      resampling_exact_image_t *image)
{
    SIXELSTATUS status;
    sixel_loader_t *loader;
    char const *loader_order;

    status = SIXEL_FALSE;
    loader = NULL;
    loader_order = "builtin!";
    memset(image, 0, sizeof(*image));
    image->allocator = allocator;

    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 image);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 loader_order);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_loader_load_file(loader,
                                    path,
                                    resampling_exact_capture);
    if (SIXEL_SUCCEEDED(status) && image->frame_count != 1) {
        status = SIXEL_BAD_INPUT;
    }

end:
    sixel_loader_unref(loader);
    return status;
}

static int
resampling_exact_method_value(char const *name)
{
    static resampling_exact_method_t const methods[] = {
        { "nearest", SIXEL_RES_NEAREST },
        { "gaussian", SIXEL_RES_GAUSSIAN },
        { "hanning", SIXEL_RES_HANNING },
        { "hamming", SIXEL_RES_HAMMING },
        { "bilinear", SIXEL_RES_BILINEAR },
        { "welsh", SIXEL_RES_WELSH },
        { "bicubic", SIXEL_RES_BICUBIC },
        { "lanczos2", SIXEL_RES_LANCZOS2 },
        { "lanczos3", SIXEL_RES_LANCZOS3 },
        { "lanczos4", SIXEL_RES_LANCZOS4 },
        { NULL, -1 }
    };
    int index;

    for (index = 0; methods[index].name != NULL; ++index) {
        if (strcmp(methods[index].name, name) == 0) {
            return methods[index].value;
        }
    }
    return -1;
}

static void
resampling_exact_report_difference(unsigned char const *actual,
                                   unsigned char const *expected,
                                   size_t size)
{
    size_t index;

    for (index = 0u; index < size; ++index) {
        if (actual[index] != expected[index]) {
            fprintf(stderr,
                    "pixel byte %lu: got %u, expected %u\n",
                    (unsigned long)index,
                    (unsigned int)actual[index],
                    (unsigned int)expected[index]);
            return;
        }
    }
}

int
test_geometry_resampling_exact(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    resampling_exact_image_t source;
    resampling_exact_image_t expected;
    resampling_exact_image_t decoded;
    unsigned char *actual;
    size_t expected_bytes;
    int method;
    int decode_mode;

    status = SIXEL_FALSE;
    allocator = NULL;
    memset(&source, 0, sizeof(source));
    memset(&expected, 0, sizeof(expected));
    memset(&decoded, 0, sizeof(decoded));
    actual = NULL;
    expected_bytes = 0u;

    if (argc != 4) {
        fprintf(stderr,
                "usage: %s METHOD INPUT.ppm EXPECTED.ppm\n"
                "       %s decode INPUT.six EXPECTED.ppm\n",
                argv[0],
                argv[0]);
        return EXIT_FAILURE;
    }
    decode_mode = strcmp(argv[1], "decode") == 0;
    method = decode_mode ? -1 : resampling_exact_method_value(argv[1]);
    if (!decode_mode && method < 0) {
        fprintf(stderr, "unknown resampling method: %s\n", argv[1]);
        return EXIT_FAILURE;
    }

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (decode_mode) {
        status = resampling_exact_decode(argv[2], allocator, &decoded);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = resampling_exact_load(argv[3], allocator, &expected);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        expected_bytes = (size_t)expected.width *
                         (size_t)expected.height * 3u;
        if (decoded.width != expected.width ||
            decoded.height != expected.height ||
            decoded.stride != expected.stride ||
            memcmp(decoded.pixels,
                   expected.pixels,
                   expected_bytes) != 0) {
            fprintf(stderr,
                    "decoded dimensions: got %dx%d, expected %dx%d\n",
                    decoded.width,
                    decoded.height,
                    expected.width,
                    expected.height);
            if (decoded.width == expected.width &&
                decoded.height == expected.height &&
                decoded.stride == expected.stride) {
                resampling_exact_report_difference(decoded.pixels,
                                                   expected.pixels,
                                                   expected_bytes);
            }
            status = SIXEL_LOGIC_ERROR;
        }
        goto end;
    }
    status = resampling_exact_load(argv[2], allocator, &source);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = resampling_exact_load(argv[3], allocator, &expected);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    expected_bytes = (size_t)expected.width *
                     (size_t)expected.height * 3u;
    actual = (unsigned char *)sixel_allocator_malloc(allocator,
                                                      expected_bytes);
    if (actual == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    status = sixel_helper_scale_image(actual,
                                      source.pixels,
                                      source.width,
                                      source.height,
                                      source.pixelformat,
                                      expected.width,
                                      expected.height,
                                      method,
                                      allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (memcmp(actual, expected.pixels, expected_bytes) != 0) {
        resampling_exact_report_difference(actual,
                                           expected.pixels,
                                           expected_bytes);
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, actual);
    sixel_allocator_free(allocator, source.pixels);
    sixel_allocator_free(allocator, expected.pixels);
    sixel_allocator_free(allocator, decoded.pixels);
    sixel_allocator_unref(allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "exact resampling comparison failed: %s\n",
                sixel_helper_format_error(status));
    }
    return SIXEL_SUCCEEDED(status) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
