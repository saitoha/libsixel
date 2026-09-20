/* SPDX-License-Identifier: MIT */
/* In-memory T.81 streams with analytic sample values. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "jpeg_dnl_test_common.h"
#include "jpeg_lossless_test_common.h"
#include "loader_builtin_memory_test_common.h"

static void
jd_marker(edge_writer_t *w, unsigned int marker)
{
    edge_put_u8(w, 255u);
    edge_put_u8(w, marker);
}

static void
jd_sos(edge_writer_t *w, int spectral_start, int spectral_end)
{
    jd_marker(w, 0xdau);
    edge_put_u16be(w, 8u);
    edge_put_u8(w, 1u);
    edge_put_u8(w, 1u);
    edge_put_u8(w, 0u);
    edge_put_u8(w, (unsigned int)spectral_start);
    edge_put_u8(w, (unsigned int)spectral_end);
    edge_put_u8(w, 0u);
}

static size_t
jd_build(unsigned char *buffer, size_t capacity, int variant)
{
    static unsigned char const fake_dnl[] = { 255, 220, 0, 4, 0, 1 };
    edge_writer_t w;
    int i;
    int table;
    int progressive;

    w.buffer = buffer;
    w.capacity = capacity;
    w.length = 0u;
    w.failed = 0;
    progressive = variant == 3 || variant == 13;
    jd_marker(&w, 0xd8u);
    jd_marker(&w, progressive ? 0xc2u : variant == 2 ? 0xc1u : 0xc0u);
    edge_put_u16be(&w, 11u);
    edge_put_u8(&w, variant == 2 ? 12u : 8u);
    edge_put_u16be(&w, 9u);
    edge_put_u16be(&w, 9u);
    edge_put_u8(&w, 1u);
    edge_put_u8(&w, 1u);
    edge_put_u8(&w, 0x11u);
    edge_put_u8(&w, 0u);
    /* A marker-shaped byte string in COM must never supply the height. */
    jd_marker(&w, 0xfeu);
    edge_put_u16be(&w, 8u);
    edge_put_bytes(&w, fake_dnl, sizeof(fake_dnl));
    jd_marker(&w, 0xdbu);
    edge_put_u16be(&w, 67u);
    edge_put_u8(&w, 0u);
    for (i = 0; i < 64; ++i) {
        edge_put_u8(&w, 1u);
    }
    for (table = 0; table < 2; ++table) {
        jd_marker(&w, 0xc4u);
        edge_put_u16be(&w, table == 0 ? 21u : 20u);
        edge_put_u8(&w, table == 0 ? 0u : 16u);
        /* DC: 0 -> category 0, 10 -> category 8. AC: 0 -> EOB. */
        for (i = 0; i < 16; ++i) {
            edge_put_u8(&w, i == 0 || (table == 0 && i == 1) ? 1u : 0u);
        }
        edge_put_u8(&w, 0u);
        if (table == 0) {
            edge_put_u8(&w, 8u);
        }
    }
    if (variant == 17) {
        jd_marker(&w, 0xddu);
        edge_put_u16be(&w, 4u);
        edge_put_u16be(&w, 2u);
    }
    jd_sos(&w, 0, progressive ? 0 : 63);
    if (variant == 6 || variant == 21) {
        /* Three zero blocks, then DC 255 and AC EOB: FF is byte-stuffed.
         * IDCT(DC=255) gives 128 + round(255/8) = 160 in the final block. */
        edge_put_u8(&w, 2u);
        edge_put_u8(&w, 255u);
        edge_put_u8(&w, 0u);
        edge_put_u8(&w, 127u);
    } else if (variant == 17) {
        edge_put_u8(&w, 15u);
        jd_marker(&w, 0xd0u);
        edge_put_u8(&w, 15u);
    } else {
        /* Four blocks: DC=0 and EOB in sequential, DC=0 in progressive. */
        edge_put_u8(&w, progressive ? 15u : 0u);
    }
    if (progressive) {
        jd_sos(&w, 1, 63);
        edge_put_u8(&w, 15u);
    }
    jd_marker(&w, 0xd9u);
    return w.failed ? 0u : w.length;
}

/* Transform a known-height stream without recoding its entropy. This test
 * utility walks the original first scan independently of the decoder. */
static int
jd_defer(unsigned char *data, size_t *size, size_t capacity,
         size_t *sof, size_t *sos, size_t *dnl)
{
    size_t p;
    size_t length;
    int marker;
    int scan;
    unsigned char high;
    unsigned char low;

    p = 2u;
    scan = 0;
    *sof = 0u;
    *sos = 0u;
    while (p + 1u < *size) {
        if (scan && data[p] != 255u) {
            ++p;
            continue;
        }
        if (data[p] != 255u) {
            return 1;
        }
        marker = data[p + 1u];
        if (scan && (marker == 0 || (marker >= 0xd0 && marker <= 0xd7))) {
            p += 2u;
            continue;
        }
        if (scan) {
            break;
        }
        if (p + 4u > *size) {
            return 1;
        }
        length = ((size_t)data[p + 2u] << 8) | data[p + 3u];
        if (length < 2u || length > *size - p - 2u) {
            return 1;
        }
        if (marker >= 0xc0 && marker <= 0xc3) {
            *sof = p;
        }
        if (marker == 0xda) {
            *sos = p;
            scan = 1;
        }
        p += length + 2u;
    }
    if (*sof == 0u || *sos == 0u || p + 1u >= *size ||
        *size + 6u > capacity) {
        return 1;
    }
    high = data[*sof + 5u];
    low = data[*sof + 6u];
    data[*sof + 5u] = 0u;
    data[*sof + 6u] = 0u;
    memmove(data + p + 6u, data + p, *size - p);
    data[p] = 255u;
    data[p + 1u] = 220u;
    data[p + 2u] = 0u;
    data[p + 3u] = 4u;
    data[p + 4u] = high;
    data[p + 5u] = low;
    *size += 6u;
    *dnl = p;
    return 0;
}

static int
jd_expect(unsigned char const *data, size_t size, int variant)
{
    static unsigned char const gray[9] = {
        128, 160, 112, 192, 144, 208, 176, 240, 96
    };
    static unsigned char const rgb[9] = {
        100, 140, 180, 220, 60, 16, 64, 200, 128
    };
    edge_frame_probe_t probe;
    edge_loader_options_t options;
    SIXELSTATUS status;
    size_t i;
    size_t count;
    int width;
    int height;
    int is_float;
    float actual;
    float expected;

    memset(&probe, 0, sizeof(probe));
    edge_loader_options_init(&options);
    options.require_static = 1;
    options.set_prefer_float32 = 1;
    is_float = variant == 1 || variant == 2 || variant == 4 || variant == 5;
    options.prefer_float32 = is_float;
    status = SIXEL_FALSE;
    if (edge_load_buffer_options("JPEG DNL", data, size, &options,
                                 &probe, &status) != 0 ||
        SIXEL_FAILED(status)) {
        fprintf(stderr, "JPEG DNL variant %d failed: %s\n", variant,
                sixel_helper_get_additional_message());
        return 1;
    }
    width = variant == 7 ? 64 : variant == 4 || variant == 5 ? 3 : 9;
    height = variant == 7 ? 64 : variant == 4 ? 3 : variant == 5 ? 1 : 9;
    count = (size_t)width * (size_t)height * 3u;
    if (probe.callback_count != 1 || probe.width[0] != width ||
        probe.height[0] != height ||
        probe.colorspace[0] != SIXEL_COLORSPACE_GAMMA ||
        probe.pixelformat[0] != (is_float ? SIXEL_PIXELFORMAT_RGBFLOAT32 :
                                          SIXEL_PIXELFORMAT_RGB888) ||
        probe.rgb_size[0] != count * (is_float ? sizeof(float) : 1u)) {
        fprintf(stderr, "JPEG DNL dimensions or pixel format changed\n");
        return 1;
    }
    if (variant == 7) {
        /* Existing fixed-pixel oracle, not derived from today's decoder. */
        return edge_digest_bytes(probe.rgb[0], count) !=
            UINT64_C(0xade587659436a76f);
    }
    for (i = 0; i < count; ++i) {
        expected = variant == 2 ? 2048.0f : 128.0f;
        if (variant == 4) {
            expected = gray[i / 3u];
        } else if (variant == 5) {
            expected = rgb[i];
        } else if (variant == 6 && i / 3u == 80u) {
            expected = 160.0f;
        }
        if (is_float) {
            expected /= variant == 2 ? 4095.0f : 255.0f;
            memcpy(&actual, probe.rgb[0] + i * sizeof(float), sizeof(float));
        } else {
            actual = probe.rgb[0][i];
        }
        if (!isfinite(actual) || fabsf(actual - expected) > 0.0000001f) {
            fprintf(stderr, "JPEG DNL sample %lu: %.9g != %.9g\n",
                    (unsigned long)i, (double)actual, (double)expected);
            return 1;
        }
    }
    return 0;
}

int
jpeg_dnl_test(int variant)
{
    unsigned char data[16384];
    unsigned char marker[6];
    size_t size;
    size_t sof;
    size_t sos;
    size_t dnl;
    size_t i;

    size = 0u;
    if (variant == 4) {
        size = jpeg_lossless_build_gray_predictor(data, sizeof(data), 4, 2);
    } else if (variant == 5) {
        size = jpeg_lossless_build_rgb_restart(data, sizeof(data));
    } else if (variant == 7) {
        if (edge_read_fixture(
                "/tests/data/inputs/formats/snake-jpeg-8bit-ycbcr-prog420.jpg",
                data, sizeof(data), &size) != 0) {
            return 1;
        }
    } else {
        size = jd_build(data, sizeof(data), variant);
    }
    if (size == 0u ||
        jd_defer(data, &size, sizeof(data), &sof, &sos, &dnl) != 0) {
        return 1;
    }
    switch (variant) {
    case 8: /* Missing DNL, with a decoy still present inside COM. */
        memmove(data + dnl, data + dnl + 6u, size - dnl - 6u);
        size -= 6u;
        break;
    case 9:
        data[dnl + 5u] = 0u;
        break;
    case 10:
        data[dnl + 3u] = 3u;
        break;
    case 11:
        /* Each truncation of the marker's length/height fields must fail. */
        for (i = dnl; i < dnl + 6u; ++i) {
            if (edge_expect_failure("truncated DNL", data, i) != 0) {
                return 1;
            }
        }
        return 0;
    case 12:
        memcpy(marker, data + dnl, sizeof(marker));
        memmove(data + sos + 6u, data + sos, dnl - sos);
        memcpy(data + sos, marker, sizeof(marker));
        break;
    case 13:
        memcpy(marker, data + dnl, sizeof(marker));
        memmove(data + dnl, data + dnl + 6u, size - dnl - 8u);
        memcpy(data + size - 8u, marker, sizeof(marker));
        break;
    case 14:
        memmove(data + dnl + 6u, data + dnl, size - dnl);
        size += 6u;
        break;
    case 15:
        data[dnl + 5u] = 1u;
        break;
    case 16:
    case 17:
        data[dnl + 5u] = 17u;
        break;
    case 18:
        /* B.2.5 permits lengths above four; skip bounded extension bytes. */
        memmove(data + dnl + 8u, data + dnl + 6u, size - dnl - 6u);
        data[dnl + 3u] = 6u;
        data[dnl + 6u] = 255u;
        data[dnl + 7u] = 220u;
        size += 2u;
        break;
    case 19:
    case 20:
        data[sof + 6u] = variant == 19 ? 9u : 8u;
        break;
    case 21:
        /* Keep the DC category but remove its required amplitude bytes. */
        memmove(data + dnl - 3u, data + dnl, size - dnl);
        size -= 3u;
        break;
    case 22:
        /* Marker fill bytes belong to the marker, not to entropy data. */
        memmove(data + dnl + 2u, data + dnl, size - dnl);
        data[dnl] = 255u;
        data[dnl + 1u] = 255u;
        size += 2u;
        break;
    case 23:
        size = dnl + 6u;
        break;
    case 24:
        /* The COM after SOF claims bytes beyond the complete input. */
        data[sof + 15u] = 255u;
        data[sof + 16u] = 255u;
        break;
    default:
        break;
    }
    if ((variant >= 8 && variant <= 17) || variant == 20 ||
        variant == 21 || variant >= 23) {
        return edge_expect_failure("invalid JPEG DNL", data, size);
    }
    return jd_expect(data, size, variant);
}
