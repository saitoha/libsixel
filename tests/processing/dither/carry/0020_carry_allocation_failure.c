/* SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/dither-carry.h"
#include "src/lookup-policy.h"
static int carry_fail_armed;
static int carry_fail_calls;
static void *
carry_fail_calloc(size_t count, size_t size)
{
    if (carry_fail_armed) {
        ++carry_fail_calls;
        return NULL;
    }
    return calloc(count, size);
}
static int
carry_fail_lookup(sixel_lookup_policy_interface_t const *self,
                   unsigned char const *pixel, int hint)
{
    (void)self;
    (void)pixel;
    (void)hint;
    return 0;
}
int
test_carry_0020(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_policy_apply_request_t request;
    sixel_lookup_policy_interface_t lookup;
    sixel_lookup_policy_vtbl_t vtbl;
    unsigned char pixels[3] = {128,128,128};
    unsigned char palette[3] = {0,0,0};
    sixel_index_t result[1] = {99};
    SIXELSTATUS status;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    dither = NULL;
    ok = 0;
    carry_fail_armed = carry_fail_calls = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL,
            carry_fail_calloc, NULL, NULL))) goto end;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 1, allocator))) goto end;
    memset(&request, 0, sizeof(request));
    memset(&vtbl, 0, sizeof(vtbl));
    vtbl.map_pixel_hint = carry_fail_lookup;
    lookup.vtbl = &vtbl;
    request.dither = dither;
    request.data = pixels;
    request.result = result;
    request.palette = palette;
    request.width = request.height = 1;
    request.depth = 3;
    request.lookup_policy = &lookup;
    carry_fail_armed = 1;
    status = sixel_dither_carry_apply(&request, SIXEL_DIFFUSE_FS);
    carry_fail_armed = 0;
    ok = status == SIXEL_BAD_ALLOCATION && carry_fail_calls == 1
        && result[0] == 99 && pixels[0] == 128;
end:
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
