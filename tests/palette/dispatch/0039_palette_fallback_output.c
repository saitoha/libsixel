/*
 * SPDX-License-Identifier: MIT
 *
 * Compare lookup-table and shift-based expansion for packed 1-bit pixels.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include <sixel.h>

static int
test_palette_fallback_output_setenv(char const *name, char const *value)
{
#if defined(HAVE__PUTENV_S)
    return _putenv_s(name, value);
#elif defined(HAVE_SETENV)
    extern int setenv(char const *name, char const *value, int overwrite);

    return setenv(name, value, 1);
#else
    (void)name;
    (void)value;

    return -1;
#endif
}

int
test_palfb_0007_output(int argc, char **argv)
{
    static char const disable_tables[] =
        "_SIXEL_TEST_PALETTE_DISABLE_TABLES";
    unsigned char src[6];
    unsigned char table_output[34];
    unsigned char fallback_output[34];
    SIXELSTATUS status;
    int table_pixelformat;
    int fallback_pixelformat;

    (void)argc;
    (void)argv;

    src[0] = 0xaau;
    src[1] = 0x55u;
    src[2] = 0x80u;
    src[3] = 0x00u;
    src[4] = 0xffu;
    src[5] = 0x00u;
    table_pixelformat = SIXEL_PIXELFORMAT_G1;
    fallback_pixelformat = SIXEL_PIXELFORMAT_G1;

    if (test_palette_fallback_output_setenv(disable_tables, "0") != 0) {
        return EXIT_FAILURE;
    }
    status = sixel_helper_normalize_pixelformat(
        table_output,
        &table_pixelformat,
        src,
        SIXEL_PIXELFORMAT_G1,
        17,
        2);
    if (SIXEL_FAILED(status) ||
        table_pixelformat != SIXEL_PIXELFORMAT_G8) {
        return EXIT_FAILURE;
    }

    if (test_palette_fallback_output_setenv(disable_tables, "1") != 0) {
        return EXIT_FAILURE;
    }
    status = sixel_helper_normalize_pixelformat(
        fallback_output,
        &fallback_pixelformat,
        src,
        SIXEL_PIXELFORMAT_G1,
        17,
        2);
    if (SIXEL_FAILED(status) ||
        fallback_pixelformat != SIXEL_PIXELFORMAT_G8 ||
        memcmp(table_output,
               fallback_output,
               sizeof(table_output)) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
