/* SPDX-License-Identifier: MIT */
/* Inspect external libpng getters without using libsixel loader output. */
#ifndef PNG_METADATA_TEST_COMMON_H
#define PNG_METADATA_TEST_COMMON_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if HAVE_LIBPNG
#include <png.h>
#include <setjmp.h>
#include <string.h>
typedef struct {
    unsigned char const *bytes;
    size_t size;
    size_t position;
} png_metadata_read_t;

typedef struct {
    int gamma;
    int chrm;
    int icc;
    int source_color_work;
    double gamma_value;
} png_metadata_test_t;

static void
png_metadata_read_bytes(png_structp png, png_bytep output, png_size_t size)
{
    png_metadata_read_t *state;

    state = (png_metadata_read_t *)png_get_io_ptr(png);
    if (size > state->size - state->position) {
        png_error(png, "PNG cHRM test read bounds");
    }
    memcpy(output, state->bytes + state->position, size);
    state->position += size;
}

static int
png_metadata_read(unsigned char const *bytes, size_t size,
                    png_metadata_test_t *metadata)
{
    png_structp png;
    png_infop info;
    png_metadata_read_t state;
    png_charp name;
    png_bytep profile;
    png_uint_32 profile_size;
    double gamma;
    double xy[8];
    int compression;
    size_t offset;
    size_t length;

    memset(metadata, 0, sizeof(*metadata));
    /* Storage selection sees source chunks even when a libpng version
     * rejects their values. Never use this presence scan as color metadata. */
    offset = 8u;
    while (offset <= size && size - offset >= 12u) {
        length = ((size_t)bytes[offset] << 24)
            | ((size_t)bytes[offset + 1u] << 16)
            | ((size_t)bytes[offset + 2u] << 8) | bytes[offset + 3u];
        if (length > size - offset - 12u ||
            memcmp(bytes + offset + 4u, "IDAT", 4u) == 0) {
            break;
        }
        if (memcmp(bytes + offset + 4u, "gAMA", 4u) == 0 ||
            memcmp(bytes + offset + 4u, "cHRM", 4u) == 0 ||
            memcmp(bytes + offset + 4u, "iCCP", 4u) == 0 ||
            memcmp(bytes + offset + 4u, "sRGB", 4u) == 0) {
            metadata->source_color_work = 1;
        }
        offset += length + 12u;
    }
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        return 1;
    }
    info = png_create_info_struct(png);
    if (info == NULL) {
        png_destroy_read_struct(&png, NULL, NULL);
        return 1;
    }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        return 1;
    }
    state.bytes = bytes;
    state.size = size;
    state.position = 0u;
    png_set_read_fn(png, &state, png_metadata_read_bytes);
#ifdef PNG_SKIP_sRGB_CHECK_PROFILE
    png_set_option(png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif
    png_read_info(png, info);
    metadata->gamma = png_get_gAMA(png, info, &gamma) == PNG_INFO_gAMA;
    metadata->gamma_value = metadata->gamma ? gamma : 0.0;
    metadata->chrm = png_get_cHRM(png, info, &xy[0], &xy[1], &xy[2], &xy[3],
        &xy[4], &xy[5], &xy[6], &xy[7]) == PNG_INFO_cHRM;
    metadata->icc = png_get_iCCP(png, info, &name, &compression, &profile,
                                 &profile_size) == PNG_INFO_iCCP;
    png_destroy_read_struct(&png, &info, NULL);
    return 0;
}
#endif
#endif
