/*
 * Verify GD can_try policy for delegated and accepted formats.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "src/chunk.h"
#include "src/loader-gd.h"
#include "tests/loader/pixelformat_test_common.h"

#if HAVE_GD
static int
expect_can_try(unsigned char const *buffer,
               size_t size,
               int expected,
               char const *label)
{
    sixel_chunk_t chunk;
    int actual;

    memset(&chunk, 0, sizeof(chunk));
    chunk.buffer = (unsigned char *)buffer;
    chunk.size = size;
    actual = loader_can_try_gd(&chunk);
    if (actual != expected) {
        fprintf(stderr,
                "loader_can_try_gd mismatch for %s: got=%d expected=%d\n",
                label,
                actual,
                expected);
        return 1;
    }

    return 0;
}
#endif

int
test_loader_0057_loader_gd_can_try_policy(int argc, char **argv)
{
#if HAVE_GD
    static unsigned char const gif_data[] = {
        'G', 'I', 'F', '8', '9', 'a'
    };
    static unsigned char const png_interlaced_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x02u, 0x00u, 0x00u, 0x01u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const png_plain_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const png_highdepth_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x10u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const png_apng_data[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 'I', 'H', 'D', 'R',
        0x00u, 0x00u, 0x00u, 0x01u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x08u, 0x02u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x08u, 'a', 'c', 'T', 'L',
        0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x00u, 0x00u, 0x00u, 0x00u
    };
    static unsigned char const jpeg_data[] = {
        0xffu, 0xd8u, 0xffu, 0xe0u, 0x00u, 0x10u, 'J', 'F', 'I', 'F'
    };
    static unsigned char const unknown_data[] = {
        'N', 'O', 'T', '_', 'I', 'M', 'G'
    };
    int result;
#endif

    (void)argc;
    (void)argv;

#if HAVE_GD
    result = 0;
    if (loader_can_try_gd(NULL) != 0) {
        fprintf(stderr, "loader_can_try_gd should reject null chunk\n");
        result = 1;
    }
    if (expect_can_try(gif_data, sizeof(gif_data), 0, "gif") != 0) {
        result = 1;
    }
    if (expect_can_try(png_interlaced_data,
                       sizeof(png_interlaced_data),
                       0,
                       "png-interlaced") != 0) {
        result = 1;
    }
    if (expect_can_try(png_plain_data,
                       sizeof(png_plain_data),
                       1,
                       "png-plain") != 0) {
        result = 1;
    }
    if (expect_can_try(png_highdepth_data,
                       sizeof(png_highdepth_data),
                       1,
                       "png-highdepth") != 0) {
        result = 1;
    }
    if (expect_can_try(png_apng_data,
                       sizeof(png_apng_data),
                       0,
                       "png-apng") != 0) {
        result = 1;
    }
    if (expect_can_try(jpeg_data, sizeof(jpeg_data), 1, "jpeg") != 0) {
        result = 1;
    }
    if (expect_can_try(unknown_data,
                       sizeof(unknown_data),
                       0,
                       "unknown") != 0) {
        result = 1;
    }

    return result;
#else
    fprintf(stderr, "GD loader unavailable\n");
    return SIXEL_TEST_SKIP;
#endif
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
