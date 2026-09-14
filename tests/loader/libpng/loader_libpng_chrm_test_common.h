/* SPDX-License-Identifier: MIT */
/* Observe the external getter gate, independently of libsixel conversion. */
#ifndef LOADER_LIBPNG_CHRM_TEST_COMMON_H
#define LOADER_LIBPNG_CHRM_TEST_COMMON_H
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#if HAVE_LIBPNG
#include <png.h>
#include <setjmp.h>
#include <string.h>
#include "src/chunk-factory.h"
#include "tests/data/color-numeric/png-chrm.h"

typedef struct {
    color_png_chrm_t const *source;
    size_t position;
} png_chrm_read_t;

typedef struct {
    int gamma;
    int chrm;
    int icc;
} png_chrm_metadata_t;

static void
png_chrm_read_bytes(png_structp png, png_bytep output, png_size_t size)
{
    png_chrm_read_t *state;

    state = (png_chrm_read_t *)png_get_io_ptr(png);
    if (size > state->source->size - state->position) {
        png_error(png, "PNG cHRM test read bounds");
    }
    memcpy(output, state->source->bytes + state->position, size);
    state->position += size;
}

static int
png_chrm_read_metadata(color_png_chrm_t const *source,
                        png_chrm_metadata_t *metadata)
{
    png_structp png;
    png_infop info;
    png_chrm_read_t state;
    png_charp name;
    png_bytep profile;
    png_uint_32 profile_size;
    double gamma;
    double xy[8];
    int compression;

    memset(metadata, 0, sizeof(*metadata));
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
    state.source = source;
    state.position = 0u;
    png_set_read_fn(png, &state, png_chrm_read_bytes);
#ifdef PNG_SKIP_sRGB_CHECK_PROFILE
    png_set_option(png, PNG_SKIP_sRGB_CHECK_PROFILE, PNG_OPTION_ON);
#endif
    png_read_info(png, info);
    metadata->gamma = png_get_gAMA(png, info, &gamma) == PNG_INFO_gAMA;
    /* Supported libpng versions may withhold more than the invalid cHRM.
     * The expected numeric interpretation follows getters, never raw revival.
     * Every fixture has the same original gAMA integer when it is present. */
    if (metadata->gamma && gamma != 0.25 && !source->srgb_priority &&
        !source->icc_priority) {
        png_destroy_read_struct(&png, &info, NULL);
        return 1;
    }
    metadata->chrm = png_get_cHRM(png, info, &xy[0], &xy[1], &xy[2], &xy[3],
        &xy[4], &xy[5], &xy[6], &xy[7]) == PNG_INFO_cHRM;
    metadata->icc = png_get_iCCP(png, info, &name, &compression, &profile,
                                 &profile_size) == PNG_INFO_iCCP;
    png_destroy_read_struct(&png, &info, NULL);
    return 0;
}
#endif /* HAVE_LIBPNG */
#endif /* LOADER_LIBPNG_CHRM_TEST_COMMON_H */
