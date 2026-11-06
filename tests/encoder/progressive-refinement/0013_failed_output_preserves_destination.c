/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/encoder.h"

int
test_progressive_0013(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    char *previous;
    char *path_copy;
    size_t path_size;
    uint64_t revision;
    int descriptor;
    int ok;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    encoder = NULL;
    path_copy = NULL;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    /* PNG destinations are only recorded here, not opened or created. */
    ok = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTFILE,
                              "png:preserved.png") == SIXEL_OK;
    previous = encoder->png_output_path;
    /* The stored path can already be in the host CRT's native syntax. */
    if (!ok || previous == NULL) {
        sixel_encoder_unref(encoder);
        return EXIT_FAILURE;
    }
    path_size = strlen(previous) + 1u;
    path_copy = malloc(path_size);
    if (path_copy == NULL) {
        sixel_encoder_unref(encoder);
        return EXIT_FAILURE;
    }
    memcpy(path_copy, previous, path_size);
    descriptor = encoder->outfd;
    revision = encoder->configuration_revision;
    /* A regular fixture cannot serve as the parent of an output file.
     * Unlike ".", this does not depend on native directory-name syntax. */
    ok = ok && SIXEL_FAILED(sixel_encoder_setopt(encoder,
                            SIXEL_OPTFLAG_OUTFILE, argv[1]));
    ok = ok && encoder->output_is_png &&
        encoder->png_output_path == previous && previous != NULL &&
        strcmp(previous, path_copy) == 0 &&
        encoder->outfd == descriptor &&
        encoder->configuration_revision == revision;
    if (!ok) {
        fprintf(stderr, "failed output changed the retained destination\n");
    }
    free(path_copy);
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
