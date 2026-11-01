/* SPDX-License-Identifier: MIT */
/* A rejected body remains sticky through end_image. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_writer_failed_body(int argc, char **argv)
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
            SIXEL_OK) goto end;
    f.reject = 1;
    (void)f.writer->vtbl->write(f.writer, "#0~", 3);
    f.reject = 0;
    ok = SIXEL_FAILED(f.writer->vtbl->end_image(f.writer, &f.receipt)) &&
        f.receipt == NULL;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
