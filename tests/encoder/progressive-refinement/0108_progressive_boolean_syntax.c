/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include "src/encoder.h"

int
test_progressive_0157(int argc, char **argv)
{
    static char const * const valid[] = {
        "delta:progressive=1", "delta:progressive=0", "delta:P1", "delta:P0"
    };
    static char const * const invalid[] = {
        "delta:T99:progressive=on", "delta:T99:progressive=off",
        "delta:T99:Pon", "delta:T99:Poff", "delta:T99:progressive=o",
        "delta:T99:progressive=true", "delta:T99:progressive=false",
        "delta:T99:progressive=01", "delta:T99:progressive=2",
        "delta:T99:progressive=-1", "delta:T99:progressive=",
        "delta:T99:P", "delta:T99:P01", "delta:T99:P2"
    };
    sixel_encoder_t *encoder;
    uint64_t revision;
    size_t i;
    int ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    ok = 0;
    if (sixel_encoder_new(&encoder, NULL) != SIXEL_OK) goto end;
    for (i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        if (sixel_encoder_setopt(encoder, 'Z', valid[i]) != SIXEL_OK ||
                encoder->progressive_enabled != (i % 2 == 0) ||
                !encoder->progressive_override || !encoder->sixdelta_enabled) {
            fprintf(stderr, "progressive boolean rejected %s\n", valid[i]);
            goto end;
        }
    }
    if (sixel_encoder_setopt(encoder, 'Z', "delta:T7:P1") != SIXEL_OK) {
        goto end;
    }
    revision = encoder->configuration_revision;
    /* A rejected boolean must also roll back an earlier valid suffix. */
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        if (sixel_encoder_setopt(encoder, 'Z', invalid[i]) == SIXEL_OK ||
                encoder->configuration_revision != revision ||
                encoder->sixdelta_threshold != 7 ||
                !encoder->progressive_enabled ||
                !encoder->progressive_override) {
            fprintf(stderr, "progressive boolean accepted %s\n", invalid[i]);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}
