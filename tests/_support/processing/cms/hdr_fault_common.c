/* SPDX-License-Identifier: MIT */
/* Exercise the real staging allocation without a runtime fault switch. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "src/cms-hdr.h"
#include "src/compat_stub.h"

static int hdr_fail_next;

static void *
hdr_fault_malloc(size_t size)
{
    if (hdr_fail_next) {
        hdr_fail_next = 0;
        return NULL;
    }
    return malloc(size);
}

#define sixel_cms_hdr_decode hdr_fault_decode
#define sixel_cms_hdr_to_linear hdr_fault_linear
#define sixel_cms_hdr_convert hdr_fault_convert
#define malloc hdr_fault_malloc
#include "src/cms-hdr.c"
#undef malloc
#undef sixel_cms_hdr_convert
#undef sixel_cms_hdr_to_linear
#undef sixel_cms_hdr_decode

int
cms_hdr_allocation_test(void)
{
    sixel_cms_hdr_t hdr = {SIXEL_CMS_HDR_PQ, 0, 203.0, 1000.0};
    float pixels[3] = {0.5f, 0.5f, 0.5f};
    float original[3];
    size_t i;
    int ok;

    memcpy(original, pixels, sizeof(original));
    hdr_fail_next = 1;
    ok = !hdr_fault_convert(pixels, 1u, &hdr) && !hdr_fail_next &&
        memcmp(pixels, original, sizeof(original)) == 0;
    ok = ok && hdr_fault_convert(pixels, 1u, &hdr);
    for (i = 0u; i < 3u; ++i) {
        ok = ok && fabs(pixels[i] - 0.4544123595766703413) < 3e-8;
    }
    return ok ? 0 : 1;
}
