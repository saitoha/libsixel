/* SPDX-License-Identifier: MIT */
/* Wire RGB percentages determine retained bytes. */
#include <stdlib.h>
#include "../../_support/codec/indexed_component_common.h"

int
test_indexed_wire_rgb_rounding(int argc, char **argv)
{
    sixel_wire_color_definition_t definition = {7, 2, {1, 50, 99}};
    unsigned char expected[3] = {3, 128, 252};
    sixel_wire_palette_request_t request;
    sixel_wire_palette_view_t view;
    sixel_wire_palette_t *wire;
    sixel_allocator_t *allocator;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    wire = NULL;
    ok = 0;
    memset(&request, 0, sizeof(request));
    request.definitions = &definition;
    request.count = 1;
    request.color_definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    request.colorspace = SIXEL_COLORSPACE_GAMMA;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK ||
            sixel_wire_palette_new(&wire, &request, allocator) != SIXEL_OK ||
            sixel_wire_palette_get_view(wire, &view) != SIXEL_OK) goto end;
    ok = memcmp(view.colors[0].emitted_rgb, expected, 3) == 0;
end:
    sixel_wire_palette_unref(wire);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
