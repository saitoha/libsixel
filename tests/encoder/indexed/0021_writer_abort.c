/* SPDX-License-Identifier: MIT */
/* Abort closes framing without an acceptance receipt. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_writer_abort(int argc, char **argv)
{
    indexed_fixture_t f;
    sixel_writer_image_header_t header;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    memset(&header, 0, sizeof(header));
    if (f.writer->vtbl->begin_image(f.writer, &header, &f.image->identity) !=
            SIXEL_OK || f.writer->vtbl->abort_image(f.writer) != SIXEL_OK) {
        goto end;
    }
    ok = f.writer->vtbl->end_image(f.writer, &f.receipt) == SIXEL_LOGIC_ERROR &&
        f.receipt == NULL;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
