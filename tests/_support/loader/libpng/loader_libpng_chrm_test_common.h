/* SPDX-License-Identifier: MIT */
/* Observe the external getter gate, independently of libsixel conversion. */
#ifndef LOADER_LIBPNG_CHRM_TEST_COMMON_H
#define LOADER_LIBPNG_CHRM_TEST_COMMON_H
#include "png_metadata_test_common.h"
#include "src/chunk-factory.h"
#include "tests/data/color-numeric/png-chrm.h"
#if HAVE_LIBPNG
typedef png_metadata_test_t png_chrm_metadata_t;

static int
png_chrm_read_metadata(color_png_chrm_t const *source,
                        png_chrm_metadata_t *metadata)
{
    if (png_metadata_read(source->bytes, source->size, metadata)) {
        return 1;
    }
    /* Getters can withhold more than the invalid cHRM. The fixture's
     * original gAMA value applies only when the external getter exposes it. */
    return metadata->gamma && metadata->gamma_value != 0.25 &&
        !source->srgb_priority && !source->icc_priority;
}
#endif
#endif
