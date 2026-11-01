/* SPDX-License-Identifier: MIT */
/* A failed header drains writer state for the next image. */
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_writer_reuse_after_failure(int argc, char **argv)
{
    indexed_fixture_t f;
    int ok;

    (void)argc;
    (void)argv;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    f.reject = 1;
    (void)indexed_fixture_accept(&f);
    f.reject = 0;
    ok = indexed_fixture_accept(&f) == SIXEL_OK && f.receipt != NULL;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
