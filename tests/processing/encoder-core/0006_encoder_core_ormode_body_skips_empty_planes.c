/*
 * SPDX-License-Identifier: MIT
 *
 * Lock the OR-mode body encoder contract for skipping empty bit-planes.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
test_encoder_core_0006_ormode_body_skip_empty_planes(int argc, char **argv)
{
    SIXELSTATUS status;
    sixel_output_t *output;
    sixel_index_t pixels[30];
    unsigned char palette[12];
    char const *expected;
    int expected_size;
    int pixel_index;
    int ok;

    (void)argc;
    (void)argv;

    output = NULL;
    ok = 0;
    for (pixel_index = 0; pixel_index < 30; pixel_index++) {
        pixels[pixel_index] = 1;
    }
    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 255;
    palette[4] = 0;
    palette[5] = 0;
    palette[6] = 0;
    palette[7] = 255;
    palette[8] = 0;
    palette[9] = 0;
    palette[10] = 0;
    palette[11] = 255;
    expected =
        "#0;2;0;0;0"
        "#1;2;100;0;0"
        "#2;2;0;100;0"
        "#3;2;0;0;100"
        "#1!5~$-";
    expected_size = (int)strlen(expected);

    status = sixel_output_new(&output,
                              test_encoder_core_ormode_body_write,
                              NULL,
                              NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    output->encode_policy = SIXEL_ENCODEPOLICY_SIZE;

    status = sixel_encode_body_ormode(pixels,
                                      5,
                                      6,
                                      palette,
                                      4,
                                      (-1),
                                      output);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "OR-mode empty-plane body returned %04x\n", status);
        goto end;
    }
    if (output->pos != expected_size) {
        fprintf(stderr,
                "OR-mode empty-plane body size is %d, expected %d\n",
                output->pos,
                expected_size);
        goto end;
    }
    if (memcmp(output->buffer, expected, (size_t)expected_size) != 0) {
        fprintf(stderr, "OR-mode empty-plane body bytes differ\n");
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
