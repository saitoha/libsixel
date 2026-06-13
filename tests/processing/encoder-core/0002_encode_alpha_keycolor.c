/*
 * SPDX-License-Identifier: MIT
 *
 * Regression test for keycolor-aware sparse SIXEL output from alpha-bearing
 * and palette-indexed inputs.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include "compat_stub.h"

typedef struct alpha_keycolor_payload {
    char bytes[65536];
    int size;
} alpha_keycolor_payload_t;

typedef struct alpha_keycolor_format_case {
    int pixelformat;
    char const *name;
    unsigned char transparent[4];
    unsigned char red[4];
} alpha_keycolor_format_case_t;

static int
test_alpha_keycolor_write(char *data, int size, void *priv)
{
    alpha_keycolor_payload_t *payload;
    int room;

    if (data == NULL || priv == NULL || size < 0) {
        return 1;
    }

    payload = (alpha_keycolor_payload_t *)priv;
    room = (int)sizeof(payload->bytes) - payload->size;
    if (size > room) {
        return 1;
    }
    memcpy(payload->bytes + payload->size, data, (size_t)size);
    payload->size += size;

    return 0;
}

static int
test_alpha_keycolor_contains(alpha_keycolor_payload_t const *payload,
                             char const *needle)
{
    int needle_size;
    int offset;

    if (payload == NULL || needle == NULL) {
        return 0;
    }
    needle_size = (int)strlen(needle);
    if (needle_size <= 0 || needle_size > payload->size) {
        return 0;
    }
    for (offset = 0; offset <= payload->size - needle_size; ++offset) {
        if (memcmp(payload->bytes + offset,
                   needle,
                   (size_t)needle_size) == 0) {
            return 1;
        }
    }

    return 0;
}

static int
test_alpha_keycolor_count(alpha_keycolor_payload_t const *payload,
                          char const *needle)
{
    int needle_size;
    int offset;
    int count;

    if (payload == NULL || needle == NULL) {
        return 0;
    }
    needle_size = (int)strlen(needle);
    if (needle_size <= 0 || needle_size > payload->size) {
        return 0;
    }
    count = 0;
    for (offset = 0; offset <= payload->size - needle_size; ++offset) {
        if (memcmp(payload->bytes + offset,
                   needle,
                   (size_t)needle_size) == 0) {
            ++count;
        }
    }

    return count;
}

static int
test_alpha_keycolor_encode(alpha_keycolor_payload_t *payload,
                           unsigned char *pixels,
                           int width,
                           int height,
                           int depth,
                           sixel_dither_t *dither,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    sixel_output_t *output;

    status = SIXEL_FALSE;
    output = NULL;
    memset(payload, 0, sizeof(*payload));

    status = sixel_output_new(&output,
                              test_alpha_keycolor_write,
                              payload,
                              allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_encode(pixels, width, height, depth, dither, output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "sixel_encode failed: status=%d message=%s\n",
                status,
                sixel_helper_get_additional_message());
    }

end:
    if (output != NULL) {
        sixel_output_unref(output);
    }

    return SIXEL_SUCCEEDED(status);
}

static int
test_alpha_keycolor_decode(alpha_keycolor_payload_t const *payload,
                           unsigned char **rgba,
                           int *width,
                           int *height,
                           int *channels,
                           sixel_allocator_t *allocator)
{
    SIXELSTATUS status;

    *rgba = NULL;
    *width = 0;
    *height = 0;
    *channels = 0;
    status = sixel_decode_rgba((unsigned char const *)payload->bytes,
                               (size_t)payload->size,
                               1,
                               NULL,
                               rgba,
                               width,
                               height,
                               channels,
                               allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_alpha_keycolor_load_file(alpha_keycolor_payload_t *payload,
                              char const *path)
{
    FILE *stream;
    size_t nread;
    size_t room;

    if (payload == NULL || path == NULL) {
        return 0;
    }

    memset(payload, 0, sizeof(*payload));
    stream = sixel_compat_fopen(path, "rb");
    if (stream == NULL) {
        return 0;
    }

    while (!feof(stream)) {
        room = sizeof(payload->bytes) - (size_t)payload->size;
        if (room == 0u) {
            fclose(stream);
            return 0;
        }
        nread = fread(payload->bytes + payload->size, 1u, room, stream);
        payload->size += (int)nread;
        if (ferror(stream)) {
            fclose(stream);
            return 0;
        }
    }
    fclose(stream);

    return payload->size > 0 ? 1 : 0;
}

static int
test_alpha_keycolor_make_temp_path(char *path, size_t path_size)
{
    int nwrite;

    if (path == NULL || path_size == 0u) {
        return 0;
    }
    nwrite = snprintf(path, path_size, "alpha-keycolor-XXXXXX.six");
    if (nwrite < 0 || (size_t)nwrite >= path_size) {
        return 0;
    }
    if (sixel_compat_mktemp(path, path_size) != 0) {
        return 0;
    }

    return 1;
}

static int
test_alpha_keycolor_check_sparse_red(unsigned char const *rgba,
                                     int width,
                                     int height,
                                     int channels)
{
    int x;
    int y;
    int index;
    int expected_opaque;
    unsigned char const *pixel;

    if (rgba == NULL || width != 8 || height != 12 || channels != 4) {
        return 0;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            index = y * width + x;
            pixel = rgba + index * channels;
            expected_opaque = (x < 2 || x >= 6) ? 1 : 0;
            if (expected_opaque != 0) {
                if (pixel[0] != 0xffU || pixel[1] != 0x00U ||
                    pixel[2] != 0x00U || pixel[3] != 0xffU) {
                    return 0;
                }
            } else if (pixel[3] != 0x00U) {
                return 0;
            }
        }
    }

    return 1;
}

static int
test_alpha_keycolor_check_two_pixel_near_red(unsigned char const *rgba,
                                             int width,
                                             int height,
                                             int channels,
                                             int expect_transparent)
{
    if (rgba == NULL || width != 2 || height != 1 || channels != 4) {
        return 0;
    }
    if (expect_transparent != 0) {
        if (rgba[3] != 0x00U) {
            return 0;
        }
    } else if (rgba[3] == 0x00U) {
        return 0;
    }
    if (rgba[4] < 0xf0U || rgba[5] > 0x08U ||
        rgba[6] > 0x08U || rgba[7] != 0xffU) {
        return 0;
    }

    return 1;
}

static int
test_alpha_keycolor_pal8(sixel_allocator_t *allocator)
{
    static unsigned char const palette[6] = {
        0x00U, 0x00U, 0x00U, 0xffU, 0x00U, 0x00U
    };
    SIXELSTATUS status;
    sixel_dither_t *dither;
    alpha_keycolor_payload_t payload;
    unsigned char pixels[96];
    unsigned char *rgba;
    int width;
    int height;
    int channels;
    int x;
    int y;

    status = SIXEL_FALSE;
    dither = NULL;
    rgba = NULL;
    width = 0;
    height = 0;
    channels = 0;

    for (y = 0; y < 12; ++y) {
        for (x = 0; x < 8; ++x) {
            pixels[y * 8 + x] = (x < 2 || x >= 6) ? 1U : 0U;
        }
    }

    status = sixel_dither_new(&dither, 2, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_PAL8);
    sixel_dither_set_transparent(dither, 0);
    sixel_dither_set_palette(dither, (unsigned char *)palette);

    if (!test_alpha_keycolor_encode(&payload,
                                    pixels,
                                    8,
                                    12,
                                    1,
                                    dither,
                                    allocator)) {
        fprintf(stderr, "PAL8 encode failed\n");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (!test_alpha_keycolor_contains(&payload, "\033P0;1q") ||
        !test_alpha_keycolor_contains(&payload, "\"1;1;8;12") ||
        !test_alpha_keycolor_contains(&payload, "#1;2;100;0;0") ||
        test_alpha_keycolor_count(&payload, "#1") < 2) {
        fprintf(stderr, "PAL8 keycolor palette contract failed\n");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (!test_alpha_keycolor_decode(&payload,
                                    &rgba,
                                    &width,
                                    &height,
                                    &channels,
                                    allocator)) {
        fprintf(stderr, "PAL8 decode failed\n");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (!test_alpha_keycolor_check_sparse_red(rgba, width, height, channels)) {
        fprintf(stderr, "PAL8 sparse alpha grid mismatch\n");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, rgba);
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    return SIXEL_SUCCEEDED(status);
}

static int
test_alpha_keycolor_two_pixel_case(
    alpha_keycolor_format_case_t const *format,
    int keycolor_enabled,
    int expect_transparent,
    sixel_allocator_t *allocator)
{
    static unsigned char const palette[6] = {
        0x00U, 0x00U, 0x00U, 0xffU, 0x00U, 0x00U
    };
    SIXELSTATUS status;
    sixel_dither_t *dither;
    alpha_keycolor_payload_t payload;
    unsigned char pixels[8];
    unsigned char *rgba;
    int width;
    int height;
    int channels;

    status = SIXEL_FALSE;
    dither = NULL;
    rgba = NULL;
    width = 0;
    height = 0;
    channels = 0;
    memcpy(pixels, format->transparent, 4u);
    memcpy(pixels + 4, format->red, 4u);

    status = sixel_dither_new(&dither, 2, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    sixel_dither_set_pixelformat(dither, format->pixelformat);
    if (keycolor_enabled != 0) {
        sixel_dither_set_transparent(dither, 0);
    }
    sixel_dither_set_palette(dither, (unsigned char *)palette);
    if (!test_alpha_keycolor_encode(&payload,
                                    pixels,
                                    2,
                                    1,
                                    4,
                                    dither,
                                    allocator)) {
        fprintf(stderr, "%s encode failed\n", format->name);
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (!test_alpha_keycolor_decode(&payload,
                                    &rgba,
                                    &width,
                                    &height,
                                    &channels,
                                    allocator)) {
        fprintf(stderr, "%s decode failed\n", format->name);
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (width != 2 || height != 1 || channels != 4) {
        fprintf(stderr, "%s decoded geometry mismatch\n", format->name);
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (expect_transparent != 0) {
        if (rgba[3] != 0x00U) {
            fprintf(stderr, "%s alpha-zero pixel stayed opaque\n",
                    format->name);
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    } else if (rgba[3] == 0x00U) {
        fprintf(stderr, "%s alpha-zero pixel became transparent\n",
                format->name);
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (rgba[4] != 0xffU || rgba[5] != 0x00U ||
        rgba[6] != 0x00U || rgba[7] != 0xffU) {
        fprintf(stderr, "%s opaque red pixel mismatch\n", format->name);
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (keycolor_enabled == 0 &&
        test_alpha_keycolor_contains(&payload, "\033P0;1q")) {
        fprintf(stderr, "%s emitted transparent header without keycolor\n",
                format->name);
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, rgba);
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    return SIXEL_SUCCEEDED(status);
}

static int
test_alpha_keycolor_encode_bytes(sixel_allocator_t *allocator)
{
    static unsigned char pixels[8] = {
        0xffU, 0x00U, 0x00U, 0x00U,
        0xffU, 0x00U, 0x00U, 0xffU
    };
    SIXELSTATUS status;
    sixel_encoder_t *encoder;
    alpha_keycolor_payload_t payload;
    unsigned char *rgba;
    char path[256];
    int width;
    int height;
    int channels;
    int ok;

    status = SIXEL_FALSE;
    encoder = NULL;
    rgba = NULL;
    width = 0;
    height = 0;
    channels = 0;
    ok = 0;
    path[0] = '\0';

    if (!test_alpha_keycolor_make_temp_path(path, sizeof(path))) {
        fprintf(stderr, "encode_bytes temp path creation failed\n");
        goto end;
    }

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "encode_bytes encoder creation failed\n");
        goto end;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTPUT, path);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "encode_bytes output option failed\n");
        goto end;
    }
    status = sixel_encoder_encode_bytes(encoder,
                                        pixels,
                                        2,
                                        1,
                                        SIXEL_PIXELFORMAT_RGBA8888,
                                        NULL,
                                        0);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "encode_bytes RGBA failed: status=%d message=%s\n",
                status,
                sixel_helper_get_additional_message());
        goto end;
    }
    if (!test_alpha_keycolor_load_file(&payload, path)) {
        fprintf(stderr, "encode_bytes output load failed\n");
        goto end;
    }
    if (!test_alpha_keycolor_decode(&payload,
                                    &rgba,
                                    &width,
                                    &height,
                                    &channels,
                                    allocator)) {
        fprintf(stderr, "encode_bytes decode failed\n");
        goto end;
    }
    if (!test_alpha_keycolor_check_two_pixel_near_red(rgba,
                                                      width,
                                                      height,
                                                      channels,
                                                      1)) {
        fprintf(stderr, "encode_bytes alpha keycolor mismatch\n");
        goto end;
    }
    ok = 1;

end:
    sixel_allocator_free(allocator, rgba);
    if (encoder != NULL) {
        sixel_encoder_unref(encoder);
    }
    if (path[0] != '\0') {
        (void)sixel_compat_unlink(path);
    }

    return ok;
}

static int
test_alpha_keycolor_rgba_formats(sixel_allocator_t *allocator)
{
    static alpha_keycolor_format_case_t const formats[] = {
        {
            SIXEL_PIXELFORMAT_RGBA8888,
            "rgba8888",
            { 0xffU, 0x00U, 0x00U, 0x00U },
            { 0xffU, 0x00U, 0x00U, 0xffU }
        },
        {
            SIXEL_PIXELFORMAT_ARGB8888,
            "argb8888",
            { 0x00U, 0xffU, 0x00U, 0x00U },
            { 0xffU, 0xffU, 0x00U, 0x00U }
        },
        {
            SIXEL_PIXELFORMAT_BGRA8888,
            "bgra8888",
            { 0x00U, 0x00U, 0xffU, 0x00U },
            { 0x00U, 0x00U, 0xffU, 0xffU }
        },
        {
            SIXEL_PIXELFORMAT_ABGR8888,
            "abgr8888",
            { 0x00U, 0x00U, 0x00U, 0xffU },
            { 0xffU, 0x00U, 0x00U, 0xffU }
        }
    };
    size_t index;

    for (index = 0u; index < sizeof(formats) / sizeof(formats[0]);
            ++index) {
        if (!test_alpha_keycolor_two_pixel_case(&formats[index],
                                                1,
                                                1,
                                                allocator)) {
            return 0;
        }
    }
    if (!test_alpha_keycolor_two_pixel_case(&formats[0], 0, 0, allocator)) {
        return 0;
    }

    return 1;
}

static int
test_alpha_keycolor_x_formats(sixel_allocator_t *allocator)
{
    static alpha_keycolor_format_case_t const formats[] = {
        {
            SIXEL_PIXELFORMAT_XRGB8888,
            "xrgb8888",
            { 0x00U, 0xffU, 0x00U, 0x00U },
            { 0x00U, 0xffU, 0x00U, 0x00U }
        },
        {
            SIXEL_PIXELFORMAT_RGBX8888,
            "rgbx8888",
            { 0xffU, 0x00U, 0x00U, 0x00U },
            { 0xffU, 0x00U, 0x00U, 0x00U }
        },
        {
            SIXEL_PIXELFORMAT_XBGR8888,
            "xbgr8888",
            { 0x00U, 0x00U, 0x00U, 0xffU },
            { 0x00U, 0x00U, 0x00U, 0xffU }
        },
        {
            SIXEL_PIXELFORMAT_BGRX8888,
            "bgrx8888",
            { 0x00U, 0x00U, 0xffU, 0x00U },
            { 0x00U, 0x00U, 0xffU, 0x00U }
        }
    };
    size_t index;

    for (index = 0u; index < sizeof(formats) / sizeof(formats[0]);
            ++index) {
        if (!test_alpha_keycolor_two_pixel_case(&formats[index],
                                                1,
                                                0,
                                                allocator)) {
            return 0;
        }
    }

    return 1;
}

int
test_encoder_core_0002_encode_alpha_keycolor(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;

    (void)argc;
    (void)argv;
    status = SIXEL_FALSE;
    allocator = NULL;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (!test_alpha_keycolor_pal8(allocator) ||
        !test_alpha_keycolor_rgba_formats(allocator) ||
        !test_alpha_keycolor_x_formats(allocator) ||
        !test_alpha_keycolor_encode_bytes(allocator)) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    status = SIXEL_OK;

end:
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status) ? EXIT_SUCCESS : EXIT_FAILURE;
}
