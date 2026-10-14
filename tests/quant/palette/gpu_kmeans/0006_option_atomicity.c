/* SPDX-License-Identifier: MIT */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/palette-kmeans-gpu.h"
#include "src/encoder.h"
#include "src/palette-kmeans.h"

int
test_kmgpu_0006_option_atomicity(int argc, char **argv)
{
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    int ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    ok = 0;
    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_QUANTIZE_MODEL,
                                  "kmeans:backend=gpu");
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_QUANTIZE_MODEL,
                                  "kmeans:backend=invalid");
    ok = SIXEL_FAILED(status) &&
        encoder->quantize_model_kmeans_backend_override == 1 &&
        encoder->quantize_model_kmeans_backend ==
            SIXEL_PALETTE_KMEANS_BACKEND_GPU;
cleanup:
    sixel_encoder_unref(encoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
