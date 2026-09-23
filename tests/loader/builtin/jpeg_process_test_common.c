/* SPDX-License-Identifier: MIT */
/* Independent JPEG coefficient and original-sample expectations. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "jpeg_process_test_common.h"
#include "loader_builtin_memory_test_common.h"
#include "../../data/color-numeric/jpeg-process.h"

/* Walk independently generated marker streams, including entropy stuffing.
 * This is a fixture editor, never a parser used by the product. */
static size_t
jp_find(unsigned char const *data, size_t size, int wanted, int occurrence)
{
    size_t p;
    size_t length;
    size_t start;
    int marker;

    p = 0u;
    while (p + 1u < size) {
        if (data[p++] != 255u) continue;
        start = p - 1u;
        while (p < size && data[p] == 255u) ++p;
        if (p == size) break;
        marker = data[p++];
        if (marker == wanted && occurrence-- == 0) return start;
        if (marker == 0 || marker == 0xd8 || marker == 0xd9 ||
            (marker >= 0xd0 && marker <= 0xd7)) continue;
        if (size - p < 2u) break;
        length = (size_t)data[p] * 256u + data[p + 1u];
        if (length < 2u || length > size - p) break;
        p += length;
    }
    return size;
}

static int
jp_mutate(unsigned char *data, size_t *size, int mutation)
{
    size_t p;
    size_t q;
    size_t sof;
    size_t sos;
    size_t length;
    int m;
    unsigned char high, low;

    sof = *size;
    for (m = 0xc0; m <= 0xcf; ++m) {
        if (m == 0xc4 || m == 0xc8 || m == 0xcc) continue;
        p = jp_find(data, *size, m, 0);
        if (p < sof) sof = p;
    }
    sos = jp_find(data, *size, 0xda, 0);
    if (sof == *size || sos == *size) return 1;
    p = jp_find(data, *size, 0xcc, 0);
    if ((mutation >= 3 && mutation <= 8) || mutation == 40) {
        if (p == *size) return 1;
        switch (mutation) {
        case 3: data[p + 3u] = 3u; break;
        case 4: data[p + 4u] = 32u; break;
        case 5: data[p + 4u] = 4u; break;
        case 6: data[p + 5u] = 15u; break;
        case 7: data[p + 4u] = 16u; data[p + 5u] = 0u; break;
        case 8: data[p + 4u] = 16u; data[p + 5u] = 64u; break;
        default: *size = p + 5u; break;
        }
        return 0;
    }
    if (mutation == 100) {
        /* Defaults are installed at SOI even when DAC is absent. */
        for (;;) {
            p = jp_find(data, *size, 0xcc, 0);
            if (p == *size) return 0;
            length = (size_t)data[p + 2u] * 256u + data[p + 3u] + 2u;
            memmove(data + p, data + p + length, *size - p - length);
            *size -= length;
        }
    }
    if (mutation == 101 || mutation == 102) {
        p = sos + 2u + (size_t)data[sos + 2u] * 256u + data[sos + 3u];
        while (p + 1u < *size) {
            if (data[p] != 255u) { ++p; continue; }
            m = data[p + 1u];
            if (m == 0 || (m >= 0xd0 && m <= 0xd7)) {
                p += 2u;
                continue;
            }
            break;
        }
        if (p + 1u >= *size) return 1;
        high = data[sof + 5u];
        low = data[sof + 6u];
        data[sof + 5u] = 0u;
        data[sof + 6u] = mutation == 101 ? 0u : 1u;
        memmove(data + p + 6u, data + p, *size - p);
        data[p] = 255u; data[p + 1u] = 220u;
        data[p + 2u] = 0u; data[p + 3u] = 4u;
        data[p + 4u] = high; data[p + 5u] = low;
        *size += 6u;
        return 0;
    }
    if (mutation == 103) {
        memmove(data + *size, data + *size - 2u, 2u);
        data[*size - 2u] = data[*size - 1u] = 255u;
        *size += 2u;
        return 0;
    }
    q = sos + 5u + 2u * data[sos + 4u];
    switch (mutation) {
    case 1: --*size; break;
    case 2: *size -= 2u; break;
    case 9:
    case 42:
    case 43:
        p = jp_find(data, *size, 0xdb, 0);
        if (p == *size) return 1;
        if (mutation == 42) data[p + 5u] = 0;
        else if (mutation == 43) *size = p + 6u;
        else {
            length = (size_t)data[p + 2u] * 256u + data[p + 3u] + 2u;
            memmove(data + p, data + p + length, *size - p - length);
            *size -= length;
        }
        break;
    case 10: data[sof + 13u] = data[sof + 10u]; break;
    case 11: data[sof + 11u] = 1u; break;
    case 12: data[sos + 5u] = 99u; break;
    case 13: data[sos + 7u] = data[sos + 5u]; break;
    case 14: data[q] = 1u; data[q + 1u] = 63u; break;
    case 15: data[q + 1u] = 1u; break;
    case 16: data[q + 2u] = 32u; break;
    case 17: data[q + 2u] = 16u; break;
    case 18: data[q + 2u] = 14u; break;
    case 19:
        p = jp_find(data, *size, 0xdd, 0);
        if (p == *size) return 1;
        data[p + 3u] = 2u;
        break;
    case 20:
    case 21:
        p = jp_find(data, *size, 0xd0, 0);
        if (p == *size) return 1;
        if (mutation == 20) data[p + 1u] = 0xd3u;
        else {
            memmove(data + p, data + p + 2u, *size - p - 2u);
            *size -= 2u;
        }
        break;
    case 22: *size = q + 4u; break;
    case 23:
    case 24:
    case 25:
    case 26:
        p = jp_find(data, *size, 0xde, 0);
        if (p == *size) return 1;
        length = (size_t)data[p + 2u] * 256u + data[p + 3u] + 2u;
        if (mutation == 23) {
            memmove(data + p, data + p + length, *size - p - length);
            *size -= length;
        } else if (mutation == 24) {
            memmove(data + p + length, data + p, *size - p);
            *size += length;
        } else if (mutation == 25) ++data[p + 4u];
        else ++data[p + 8u];
        break;
    case 27:
    case 28:
    case 29:
    case 33:
        p = jp_find(data, *size, 0xdf, 0);
        if (p == *size) return 1;
        if (mutation == 27) data[p + 4u] = 2u;
        else if (mutation == 28) {
            memmove(data + p + 5u, data + p, *size - p);
            *size += 5u;
        } else if (mutation == 29) {
            memmove(data + sos + 5u, data + sos, *size - sos);
            memcpy(data + sos, data + p + 5u, 5u);
            *size += 5u;
        } else {
            memmove(data + *size + 3u, data + *size - 2u, 2u);
            memcpy(data + *size - 2u, data + p, 5u);
            *size += 5u;
        }
        break;
    case 30:
    case 31:
    case 32:
        p = jp_find(data, *size, 0xc5, 0);
        if (p == *size) return 1;
        if (mutation == 30) {
            memmove(data + sof, data + p, *size - p);
            *size -= p - sof;
        } else if (mutation == 31) data[p + 10u] = 99u;
        else ++data[p + 8u];
        break;
    case 36: data[q] = 0u; break;
    case 37: data[q + 2u] = data[sof + 4u]; break;
    case 38: data[sos + 6u] |= 1u; break;
    case 39: data[q] = 8u; break;
    case 44: data[q + 1u] = 1u; break;
    case 45: data[*size - 1u] = 0xf1u; break;
    case 46:
        p = sos + 2u + (size_t)data[sos + 2u] * 256u + data[sos + 3u];
        q = jp_find(data, *size, 0xdf, 0);
        if (q == *size || q <= p) return 1;
        memmove(data + p, data + q, *size - q);
        *size -= q - p;
        break;
    default: return 1;
    }
    return 0;
}

int
jpeg_process_test(int fixture, int mutation)
{
    struct jp_fixture const *f;
    edge_frame_probe_t probe;
    edge_loader_options_t options;
    unsigned char data[8192];
    SIXELSTATUS status;
    size_t size;
    size_t i;
    size_t count;
    float actual;
    float expected;
    float bound;
    int byte;

    f = jp_fixtures + fixture;
    size = f->size;
    if (size > sizeof(data)) {
        return 1;
    }
    memcpy(data, f->data, size);
    if (mutation) {
        if (size + 64u > sizeof(data) ||
            jp_mutate(data, &size, mutation) != 0) return 1;
        if (mutation < 100)
            return edge_expect_failure("invalid JPEG process", data, size);
    }
    memset(&probe, 0, sizeof(probe));
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.set_prefer_float32 = 1;
    byte = f->byte;
    options.prefer_float32 = !byte;
    status = SIXEL_FALSE;
    if (edge_load_buffer_options("JPEG process", data, size, &options,
                                 &probe, &status) != 0 ||
        SIXEL_FAILED(status)) {
        fprintf(stderr, "JPEG fixture %d: %s\n", fixture,
                sixel_helper_get_additional_message());
        return 1;
    }
    count = (size_t)f->width * (size_t)f->height * 3u;
    if (probe.callback_count != 1 || probe.width[0] != f->width ||
        probe.height[0] != f->height ||
        probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        probe.pixelformat[0] != (byte ? SIXEL_PIXELFORMAT_RGB888 :
                                       SIXEL_PIXELFORMAT_RGBFLOAT32) ||
        probe.rgb_size[0] != count * (byte ? 1u : sizeof(float))) {
        fprintf(stderr, "JPEG fixture %d geometry or representation\n",
                fixture);
        return 1;
    }
    for (i = 0u; i < count; ++i) {
        expected = (float)f->expected[i];
        bound = 0.0f;
        if (byte) {
            actual = probe.rgb[0][i];
        } else {
            expected /= (float)((1u << f->precision) - 1u);
            memcpy(&actual, probe.rgb[0] + i * sizeof(float), sizeof(float));
            /* Only normalization/storage rounding, no sample-level slack. */
            bound = fabsf(expected) * 4.0f * FLT_EPSILON;
        }
        if (!isfinite(actual) || fabsf(actual - expected) > bound) {
            fprintf(stderr, "JPEG fixture %d sample %lu: %.9g != %.9g\n",
                    fixture, (unsigned long)i, (double)actual,
                    (double)expected);
            return 1;
        }
    }
    return 0;
}
