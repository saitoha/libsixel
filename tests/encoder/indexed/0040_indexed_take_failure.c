/* SPDX-License-Identifier: MIT */
/* Failed import leaves both source allocations with the caller. */
#include <stdlib.h>
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_indexed_take_failure(int argc, char **argv)
{
    indexed_fixture_t f;
    sixel_indexed_frame_take_request_t request;
    sixel_indexed_frame_t *image;
    int ok;

    (void)argc;
    (void)argv;
    image = NULL;
    ok = 0;
    if (!indexed_fixture_init(&f)) goto end;
    memset(&request, 0, sizeof(request));
    request.description = f.image->description;
    request.indexes = f.image->indexes;
    request.indexes_size = 2 * sizeof(*request.indexes);
    request.skip = f.image->skip;
    request.skip_size = 2;
    request.indexes[0] = 3;
    ok = sixel_indexed_frame_take(&image, &request, f.allocator) ==
            SIXEL_BAD_ARGUMENT && image == NULL &&
        request.indexes == f.image->indexes && request.skip == f.image->skip;
end:
    indexed_fixture_destroy(&f);
    return ok ? 0 : 1;
}
