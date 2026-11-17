/* SPDX-License-Identifier: MIT */
#ifndef CMS_BPC_COMMON_H
#define CMS_BPC_COMMON_H
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "src/cms.h"
#include "src/compat_stub.h"

static void
bpc_word(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

/* Deliberately simple D50 diagonal primaries and y=(1-b)x+b curves.
 * Exact fixed-point slopes make the expected compensation algebraic;
 * this is not an output produced by another CMS transform. */
static size_t
bpc_profile(unsigned char bytes[512], uint32_t black, int gray)
{
    static char const *const tags[7] = {
        "rXYZ", "gXYZ", "bXYZ", "rTRC", "gTRC", "bTRC", "wtpt"
    };
    static uint32_t const white[3] = {63190u, 65536u, 54061u};
    size_t count;
    size_t offset;
    size_t i;
    size_t c;
    size_t length;
    unsigned char *entry;
    unsigned char *value;

    memset(bytes, 0, 512u);
    bytes[8] = 4u;
    memcpy(bytes + 12u, gray ? "mntrGRAYXYZ " : "mntrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    count = gray ? 2u : 7u;
    bpc_word(bytes + 128u, (uint32_t)count);
    offset = 132u + count * 12u;
    for (i = 0u; i < count; ++i) {
        entry = bytes + 132u + i * 12u;
        value = bytes + offset;
        if ((gray && i == 0u) || (!gray && i >= 3u && i <= 5u)) {
            memcpy(entry, gray ? "kTRC" : tags[i], 4u);
            length = 40u;
            memcpy(value, "para", 4u);
            value[9] = 4u;
            bpc_word(value + 12u, 65536u); /* g = 1 */
            bpc_word(value + 16u, 65536u - black); /* a = 1-b */
            bpc_word(value + 20u, black); /* b */
            /* c=d=e=f=0: every nonnegative device value uses ax+b. */
        } else {
            memcpy(entry, gray ? "wtpt" : tags[i], 4u);
            length = 20u;
            memcpy(value, "XYZ ", 4u);
            for (c = 0u; c < 3u; ++c) {
                bpc_word(value + 8u + c * 4u,
                    gray || i == 6u || i == c ? white[c] : 0u);
            }
        }
        bpc_word(entry + 4u, (uint32_t)offset);
        bpc_word(entry + 8u, (uint32_t)length);
        offset += length;
    }
    bpc_word(bytes, (uint32_t)offset);
    return offset;
}

/* Standard float mpet endpoints independently encode the same affine map. */
static size_t
bpc_mpet_profile(unsigned char bytes[512], double black)
{
    static double const white[3] = {0.9642, 1.0, 0.8249};
    unsigned char *tag;
    unsigned char *matrix;
    uint32_t bits;
    float value;
    size_t direction;
    size_t i;
    size_t c;
    size_t offset;

    memset(bytes, 0, 512u);
    bytes[8] = 4u;
    memcpy(bytes + 12u, "mntrRGB XYZ ", 12u);
    memcpy(bytes + 36u, "acsp", 4u);
    bpc_word(bytes, 324u);
    bpc_word(bytes + 128u, 2u);
    for (direction = 0u; direction < 2u; ++direction) {
        offset = 156u + 84u * direction;
        memcpy(bytes + 132u + 12u * direction,
               direction ? "B2D1" : "D2B1", 4u);
        bpc_word(bytes + 136u + 12u * direction, (uint32_t)offset);
        bpc_word(bytes + 140u + 12u * direction, 84u);
        tag = bytes + offset;
        memcpy(tag, "mpet", 4u);
        tag[9] = tag[11] = 3u;
        bpc_word(tag + 12u, 1u);
        bpc_word(tag + 16u, 24u);
        bpc_word(tag + 20u, 60u);
        matrix = tag + 24u;
        memcpy(matrix, "matf", 4u);
        matrix[9] = matrix[11] = 3u;
        for (i = 0u; i < 3u; ++i) {
            for (c = 0u; c < 3u; ++c) {
                value = i != c ? 0.0f : (float)(direction
                    ? 1.0 / (white[i] * (1.0 - black))
                    : white[i] * (1.0 - black));
                memcpy(&bits, &value, sizeof(bits));
                bpc_word(matrix + 12u + (i * 3u + c) * 4u, bits);
            }
            value = (float)(direction ? -black / (1.0 - black)
                                       : white[i] * black);
            memcpy(&bits, &value, sizeof(bits));
            bpc_word(matrix + 48u + i * 4u, bits);
        }
    }
    return 324u;
}

static inline int
cms_bpc_test(int mode)
{
    unsigned char source_bytes[512];
    unsigned char target_bytes[512];
    float input[9] = {0.0f, 0.0f, 0.0f, 0.25f, 0.5f, 0.75f,
                      1.0f, 1.0f, 1.0f};
    float output[12];
    unsigned char byte_input[9] = {0, 0, 0, 64, 128, 192, 255, 255, 255};
    unsigned char byte_output[9];
    uint16_t words[12] = {0, 0, 0, 16384, 16384, 32768, 49152, 32768,
                          65535, 65535, 65535, 65535};
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *transform;
    sixel_cms_pixel_format_t format;
    sixel_cms_pixel_format_t destination;
    sixel_cms_engine_t previous;
    sixel_cms_status_t status;
    size_t source_size;
    size_t target_size;
    size_t i;
    size_t count;
    int flags;
    int byte_mode;
    int gray_mode;
    int ok;
    double expected;
    double actual;
    double tolerance;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(SIXEL_CMS_ENGINE_BUILTIN);
    gray_mode = mode == 0 || mode == 5 || mode == 7 || mode == 8;
    source_size = bpc_profile(source_bytes, 16384u,
                               gray_mode);
    if (mode == 6 || mode == 10) {
        /* A usable RGB profile maps device black to PCS white. */
        bpc_word(source_bytes + 248u, 262144u);
    }
    if (mode == 5) {
        input[0] = 0.25f; input[1] = 0.5f; input[2] = 0.75f;
    }
    if (gray_mode && mode != 5) {
        input[0] = 0.0f; input[1] = 0.5f; input[2] = 1.0f;
    }
    target_size = bpc_profile(target_bytes, mode == 4 ? 16384u : 8192u, 0);
    if (mode == 9) {
        source_size = bpc_mpet_profile(source_bytes, 0.25);
        target_size = bpc_mpet_profile(target_bytes, 0.125);
    }
    source = sixel_cms_open_profile_from_mem(source_bytes, source_size);
    target = gray_mode && mode != 5 ? sixel_cms_create_linear_srgb_profile()
        : sixel_cms_open_profile_from_mem(target_bytes, target_size);
    byte_mode = mode == 2;
    format = byte_mode ? SIXEL_CMS_PIXELFORMAT_RGB_8 : mode == 3
        ? SIXEL_CMS_PIXELFORMAT_RGBA_16 : gray_mode
        ? SIXEL_CMS_PIXELFORMAT_GRAY_F32 : SIXEL_CMS_PIXELFORMAT_RGB_F32;
    destination = byte_mode ? SIXEL_CMS_PIXELFORMAT_RGB_8 : mode == 3
        ? SIXEL_CMS_PIXELFORMAT_RGBA_F32 : SIXEL_CMS_PIXELFORMAT_RGB_F32;
    flags = mode == 0 ? 0 : SIXEL_CMS_TRANSFORM_BLACK_POINT_COMPENSATION;
    if (mode == 3) { flags |= SIXEL_CMS_TRANSFORM_COPY_ALPHA; }
    ok = sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT",
        mode == 7 ? "absolute!" : mode == 8 ? "perceptual!"
            : mode == 10 ? "relative,absolute!" : "relative!") == 0;
    transform = sixel_cms_create_transform_ex(source, format, target,
                                               destination, flags, &status);
    memset(output, 0x5a, sizeof(output));
    ok = ok && transform != NULL;
    if (ok) {
        ok = sixel_cms_do_transform_ex(transform,
            byte_mode ? (void *)byte_input : mode == 3 ? (void *)words : input,
            byte_mode ? (void *)byte_output : output, 3u, &status);
    }
    if (mode == 6) {
        /* A white-valued black point is singular; publish no output. */
        ok = !ok && status == SIXEL_CMS_STATUS_EVALUATION_FAILED;
        for (i = 0u; ok && i < sizeof(output); ++i) {
            ok = ((unsigned char *)output)[i] == 0x5a;
        }
    } else {
        count = mode == 3 ? 12u : 9u;
        tolerance = byte_mode ? 1.0 : 4e-5;
        for (i = 0u; ok && i < count; ++i) {
            expected = byte_mode ? byte_input[i] / 255.0 : mode == 3
                ? words[i] / 65535.0 : gray_mode ? input[i / 3u] : input[i];
            if (gray_mode && mode != 5) {
                expected = 0.75 * expected + 0.25;
            }
            if (mode == 10) {
                expected = i % 3u == 1u ? 1.0 : (6.0 * expected + 1.0) / 7.0;
            }
            if (byte_mode) { expected = floor(expected * 255.0 + 0.5); }
            actual = byte_mode ? byte_output[i] : output[i];
            if (!isfinite(actual) || fabs(actual - expected) > tolerance) {
                fprintf(stderr, "BPC mode=%d sample=%zu actual=%.9g "
                    "expected=%.9g\n", mode, i, actual, expected);
                ok = 0;
            }
        }
    }
    sixel_cms_delete_transform(transform);
    sixel_cms_close_profile(source);
    sixel_cms_close_profile(target);
    sixel_cms_set_engine(previous);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    return !ok;
}
/* Native engines own black-point estimation. Test option delivery and the
 * intent boundary, not equality with builtin's neutral-axis algorithm. */
static inline int
cms_bpc_native_test(sixel_cms_engine_t engine, int relative)
{
    unsigned char bytes[512];
    size_t length;
    sixel_cms_engine_t previous;
    sixel_cms_profile_t *source;
    sixel_cms_profile_t *target;
    sixel_cms_transform_t *off;
    sixel_cms_transform_t *on;
    float input = 0.1f;
    float plain[3];
    float corrected[3];
    int ok;
    size_t i;

    previous = sixel_cms_get_engine();
    sixel_cms_set_engine(engine);
    if (sixel_cms_get_engine() != engine) {
        sixel_cms_set_engine(previous);
        return 77;
    }
    length = bpc_profile(bytes, 8192u, 1);
    bpc_word(bytes + 68u, 63190u);
    bpc_word(bytes + 72u, 65536u);
    bpc_word(bytes + 76u, 54061u);
    source = sixel_cms_open_profile_from_mem(bytes, length);
    target = sixel_cms_create_linear_srgb_profile();
    ok = sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT",
                               relative ? "relative!" : "absolute!") == 0;
    off = sixel_cms_create_transform(source, SIXEL_CMS_PIXELFORMAT_GRAY_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32, 0);
    on = sixel_cms_create_transform(source, SIXEL_CMS_PIXELFORMAT_GRAY_F32,
        target, SIXEL_CMS_PIXELFORMAT_RGB_F32,
        SIXEL_CMS_TRANSFORM_BLACK_POINT_COMPENSATION);
    ok = ok && off != NULL && on != NULL &&
        sixel_cms_transform_engine(on) == engine &&
        sixel_cms_transform_engine(off) == engine &&
        sixel_cms_do_transform(off, &input, plain, 1u) &&
        sixel_cms_do_transform(on, &input, corrected, 1u);
    for (i = 0u; ok && i < 3u; ++i) {
        ok = isfinite(plain[i]) && isfinite(corrected[i]) &&
            (relative ? corrected[i] >= 0.0f &&
                        plain[i] - corrected[i] > 0.01f
                      : fabs(plain[i] - corrected[i]) < 1e-6);
    }
    sixel_cms_delete_transform(on);
    sixel_cms_delete_transform(off);
    sixel_cms_close_profile(source);
    sixel_cms_close_profile(target);
    sixel_cms_set_engine(previous);
    (void)sixel_compat_setenv("SIXEL_LOADER_CMS_RENDERING_INTENT", "");
    return !ok;
}
#endif
