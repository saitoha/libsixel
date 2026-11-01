/* SPDX-License-Identifier: MIT */
/* Finite fixed-point coefficients can still produce NaN/Inf in pow().
 * Profiles and a one-pixel BMP are constructed in memory, without a CMM
 * oracle or fixture I/O. Every caller reports one independent observation. */
#include <math.h>
#include "tests/_support/loader/pixelformat_test_common.h"
#include "tests/_support/diagnostics/icc_numeric_common.h"
#include "tests/data/color-numeric/icc.h"
#include "src/icc-parse.h"
#include "src/cms.h"
#include "src/loader-common.h"

static void
iccn_be32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static void
iccn_le32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static unsigned int
iccn_read32(unsigned char const *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
        ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

static size_t
iccn_profile(unsigned char *bytes, int segmented, int failure)
{
    unsigned char *tag;
    unsigned char *params;
    size_t i;
    size_t size;
    unsigned int count;

    memcpy(bytes, color_icc, sizeof(color_icc));
    tag = bytes + sizeof(color_icc);
    memset(tag, 0, 56u);
    size = sizeof(color_icc) + 56u;
    iccn_be32(bytes, (unsigned int)size);
    count = iccn_read32(bytes + 128);
    for (i = 0u; i < count; ++i) {
        /* Invalid gTRC follows a valid allocated rTRC, exercising cleanup.
         * The finite control replaces every TRC with the same exact curve. */
        if (memcmp(bytes + 132u + i * 12u, "gTRC", 4u) == 0 ||
            (!failure && memcmp(bytes + 133u + i * 12u, "TRC", 3u) == 0)) {
            iccn_be32(bytes + 136u + i * 12u, sizeof(color_icc));
            iccn_be32(bytes + 140u + i * 12u, 56u);
        }
    }
    if (segmented) {
        memcpy(tag, "segm", 4u);
        iccn_be32(tag + 8, failure ? 2u : 1u);
        params = tag + 16;
        if (failure) {
            /* The first half is identity; fail only in the second segment. */
            iccn_be32(tag + 12, 0x00008000u);
            iccn_be32(tag + 20, 0x00010000u);
            tag[25] = 4u;
            params = tag + 28;
        } else {
            tag[13] = 4u;
        }
    } else {
        memcpy(tag, "para", 4u);
        tag[9] = 4u;
        params = tag + 12;
    }
    /* Type 4: g,a,b,c,d,e,f. Finite control is clamp(4*x-1).
     * NaN: negative power base with exponent 1.5.
     * Inf: an exponent of 32767 overflows after x passes the knee. */
    iccn_be32(params, failure == 1 ? 0x00018000u :
        failure == 2 ? 0x7fff0000u : 0x00010000u);
    iccn_be32(params + 4, failure == 1 ? 0x00010000u :
        failure == 2 ? 0x00020000u : 0x00040000u);
    iccn_be32(params + 8, failure == 1 ? 0xfffe0000u :
        failure == 2 ? 0u : 0xffff0000u);
    iccn_be32(params + 12, 0x00010000u);
    iccn_be32(params + 16, failure ? 0x00008000u : 0u);
    return size;
}

int
icc_numeric_parse_test(int segmented, int failure)
{
    unsigned char bytes[sizeof(color_icc) + 56u];
    sixel_icc_profile_t profile;
    unsigned char untouched[sizeof(profile)];
    size_t size;
    size_t i;
    size_t n;
    long numerator;
    unsigned int expected;
    int c;
    int ok;

    size = iccn_profile(bytes, segmented, failure);
    memset(&profile, 0xa5, sizeof(profile));
    memcpy(untouched, &profile, sizeof(profile));
    ok = sixel_icc_parse_profile(bytes, size, &profile);
    if (failure) {
        if (ok) {
            sixel_icc_profile_destroy(&profile);
            return EXIT_FAILURE;
        }
        /* Failed parsing must not publish a partially initialized object. */
        if (memcmp(untouched, &profile, sizeof(profile)) != 0) {
            return EXIT_FAILURE;
        }
        if (!sixel_icc_parse_profile(color_icc, sizeof(color_icc), &profile)) {
            return EXIT_FAILURE;
        }
        sixel_icc_profile_destroy(&profile);
        return EXIT_SUCCESS;
    }
    if (!ok) {
        return EXIT_FAILURE;
    }
    for (c = 0; c < 3; ++c) {
        n = profile.curves[c].table_length;
        if (n != 4096u || profile.curves[c].table == NULL) {
            ok = 0;
            break;
        }
        for (i = 0u; i < n; ++i) {
            /* Integer-rational oracle, independent of pow and parser math. */
            numerator = 4L * (long)i - (long)(n - 1u);
            expected = numerator <= 0 ? 0u :
                numerator >= (long)(n - 1u) ? 65535u :
                (unsigned int)((numerator * 65535L + (long)((n - 1u) / 2u)) /
                    (long)(n - 1u));
            if (profile.curves[c].table[i] != expected) {
                ok = 0;
                break;
            }
        }
    }
    sixel_icc_profile_destroy(&profile);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static SIXELSTATUS
iccn_capture(sixel_frame_t *frame, void *context)
{
    float const *rgb;
    int *calls;
    int c;
    /* P3 red -> canonical linear sRGB, from primary chromaticities.
     * ICC fixed-point serialization needs a 3e-4 error allowance. */
    double const expected[3] = {
        1.2249401762805598, -0.0420569547096882, -0.0196375545903344
    };

    calls = (int *)context;
    ++*calls;
    if (frame->pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 ||
        frame->width != 1 || frame->height != 1 ||
        frame->colorspace != SIXEL_COLORSPACE_LINEAR) {
        return SIXEL_BAD_INPUT;
    }
    rgb = (float const *)sixel_frame_get_pixels(frame);
    for (c = 0; c < 3; ++c) {
        if (!isfinite(rgb[c]) || fabs(rgb[c] - expected[c]) > 3e-4) {
            return SIXEL_BAD_INPUT;
        }
    }
    return SIXEL_OK;
}

int
icc_numeric_loader_test(int segmented)
{
    unsigned char bytes[142u + sizeof(color_icc) + 56u];
    size_t size;
    sixel_allocator_t *allocator;
    sixel_loader_component_t *loader;
    sixel_chunk_t *chunk;
    loader_probe_callback_state_t callback;
    SIXELSTATUS status;
    int engine;
    int calls;
    int result;

    allocator = NULL;
    loader = NULL;
    chunk = NULL;
    calls = 0;
    result = EXIT_FAILURE;
    engine = SIXEL_CMS_ENGINE_BUILTIN;
    callback = (loader_probe_callback_state_t){NULL, iccn_capture, &calls};
    memset(bytes, 0, sizeof(bytes));
    /* One opaque red BGR pixel followed by a standard V5 embedded profile. */
    memcpy(bytes, "BM", 2u);
    iccn_le32(bytes + 10, 138u);
    iccn_le32(bytes + 14, 124u);
    iccn_le32(bytes + 18, 1u);
    iccn_le32(bytes + 22, 1u);
    bytes[26] = 1u;
    bytes[28] = 24u;
    memcpy(bytes + 70, "DEBM", 4u);
    iccn_le32(bytes + 126, 128u);
    size = iccn_profile(bytes + 142, segmented, 1);
    iccn_le32(bytes + 130, (unsigned int)size);
    size += 142u;
    iccn_le32(bytes + 2, (unsigned int)size);
    bytes[140] = 255u;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status) || SIXEL_FAILED(create_loader_component_by_name(
            "builtin", allocator, (void **)&loader))) {
        goto end;
    }
    if (SIXEL_FAILED(sixel_loader_component_setopt(loader,
            SIXEL_LOADER_COMPONENT_OPTION_CMS_ENGINE, &engine)) ||
        SIXEL_FAILED(sixel_chunk_create_from_memory(&chunk, bytes, size,
            NULL, allocator))) {
        goto end;
    }
    status = sixel_loader_component_load(loader, chunk,
                                           capture_frame_trampoline, &callback);
    if (!SIXEL_FAILED(status) || calls != 0) {
        goto end;
    }
    chunk->vtbl->unref(chunk);
    chunk = NULL;
    /* Retry a valid profile on the same loader, and observe extended RGB. */
    memcpy(bytes + 142, color_icc, sizeof(color_icc));
    size = 142u + sizeof(color_icc);
    iccn_le32(bytes + 2, (unsigned int)size);
    iccn_le32(bytes + 130, sizeof(color_icc));
    if (SIXEL_FAILED(sixel_chunk_create_from_memory(&chunk, bytes, size,
            NULL, allocator))) {
        goto end;
    }
    status = sixel_loader_component_load(loader, chunk,
                                           capture_frame_trampoline, &callback);
    if (SIXEL_SUCCEEDED(status) && calls == 1) {
        result = EXIT_SUCCESS;
    }
end:
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    if (loader != NULL) {
        sixel_loader_component_unref(loader);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return result;
}
