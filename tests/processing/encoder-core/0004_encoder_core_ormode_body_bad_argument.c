/*
 * SPDX-License-Identifier: MIT
 *
 * Lock the OR-mode body encoder argument guard for a missing index buffer.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

#include "src/encoder-core-private.h"

static int
test_encoder_core_ormode_body_write(char *data, int size, void *priv)
{
    (void)data;
    (void)size;
    (void)priv;

    return 0;
}

int
test_encoder_core_0004_encoder_core_ormode_body_bad_argument(int argc,
                                                             char **argv)
{
    SIXELSTATUS status;
    sixel_output_t *output;
    unsigned char palette[6];
    int ok;

    (void)argc;
    (void)argv;

    output = NULL;
    ok = 0;
    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 255;
    palette[4] = 255;
    palette[5] = 255;

    status = sixel_output_new(&output,
                              test_encoder_core_ormode_body_write,
                              NULL,
                              NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_encode_body_ormode(NULL,
                                      1,
                                      1,
                                      palette,
                                      2,
                                      (-1),
                                      output);
    if (status != SIXEL_BAD_ARGUMENT) {
        fprintf(stderr,
                "OR-mode body returned %04x, expected bad argument\n",
                status);
        goto end;
    }
    if (output->pos != 0) {
        fprintf(stderr,
                "OR-mode body wrote %d bytes after bad argument\n",
                output->pos);
        goto end;
    }

    ok = 1;

end:
    if (output != NULL) {
        sixel_output_unref(output);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
