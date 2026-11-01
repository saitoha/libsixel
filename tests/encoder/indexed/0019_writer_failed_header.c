/* SPDX-License-Identifier: MIT */
/* A rejected header never issues an acceptance receipt. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_writer_failed_header(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    f.reject = 1;
    ok = SIXEL_FAILED(indexed_fixture_accept(&f)) && f.receipt == NULL;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
