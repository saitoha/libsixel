/* SPDX-License-Identifier: MIT */
/* Invalid selections preserve the current explicit interpretation. */
#include <stdlib.h>
#include <sixel.h>
#include "../../../src/decoder.h"

int test_sequence_0085(int argc, char **argv)
{
    sixel_decoder_t *decoder;
    SIXELSTATUS status;
    int ok;

    (void)argc;
    (void)argv;
    decoder = NULL;
    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    ok = decoder->progressive_input == 0;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "6");
    ok = ok && SIXEL_SUCCEEDED(status) && decoder->progressive_input == 1;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "6delta-pro");
    ok = ok && SIXEL_SUCCEEDED(status) && decoder->progressive_input == 1;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "6delta-progressive");
    ok = ok && SIXEL_SUCCEEDED(status) && decoder->progressive_input == 1;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "auto");
    ok = ok && SIXEL_FAILED(status) && decoder->progressive_input == 1;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "");
    ok = ok && SIXEL_FAILED(status) && decoder->progressive_input == 1;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, NULL);
    ok = ok && SIXEL_FAILED(status) && decoder->progressive_input == 1;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "6delta-progressive-extra");
    ok = ok && SIXEL_FAILED(status) && decoder->progressive_input == 1;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "Single");
    ok = ok && SIXEL_FAILED(status) && decoder->progressive_input == 1;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "s");
    ok = ok && SIXEL_SUCCEEDED(status) && decoder->progressive_input == 0;
    status = sixel_decoder_setopt(decoder,
        SIXEL_OPTFLAG_DECODE_INPUT_MODE, "single");
    ok = ok && SIXEL_SUCCEEDED(status) && decoder->progressive_input == 0;
    sixel_decoder_unref(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
