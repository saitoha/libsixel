/*
 * SPDX-License-Identifier: MIT
 *
 * Verify terminal policy parsing and encoder-only scope.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include <sixel.h>

typedef struct terminal_policy_case {
    char const *value;
    int should_pass;
} terminal_policy_case_t;

int
test_cli_0034_cli_terminal_policy_option(int argc, char **argv)
{
    terminal_policy_case_t cases[] = {
        { "0", 1 },
        { "1", 1 },
        { "on", 0 },
        { "2", 0 },
        { NULL, 0 }
    };
    sixel_encoder_t *encoder;
    sixel_decoder_t *decoder;
    SIXELSTATUS status;
    size_t index;
    int failed;

    (void)argc;
    (void)argv;
    encoder = NULL;
    decoder = NULL;
    failed = 0;

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status) || encoder == NULL) {
        fprintf(stderr, "failed to create encoder\n");
        return EXIT_FAILURE;
    }
    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status) || decoder == NULL) {
        fprintf(stderr, "failed to create decoder\n");
        sixel_encoder_unref(encoder);
        return EXIT_FAILURE;
    }

    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        status = sixel_encoder_setopt(encoder,
                                      SIXEL_OPTFLAG_TERMINAL_POLICY,
                                      cases[index].value);
        if (cases[index].should_pass != 0 && SIXEL_FAILED(status)) {
            fprintf(stderr, "case %zu: value should pass\n", index + 1u);
            failed = 1;
        } else if (cases[index].should_pass == 0 &&
                   SIXEL_SUCCEEDED(status)) {
            fprintf(stderr, "case %zu: value should fail\n", index + 1u);
            failed = 1;
        }
    }

    status = sixel_decoder_setopt(decoder,
                                  SIXEL_OPTFLAG_TERMINAL_POLICY,
                                  "1");
    if (SIXEL_SUCCEEDED(status)) {
        fprintf(stderr, "decoder accepted encoder terminal policy\n");
        failed = 1;
    }

    sixel_decoder_unref(decoder);
    sixel_encoder_unref(encoder);
    return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
