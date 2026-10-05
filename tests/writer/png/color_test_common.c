/* SPDX-License-Identifier: MIT */
/* Parse PNG/ICC fields independently of the production writer. Pixel
 * decoding explicitly disables loader CMS and inspects its exact paint mask. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "src/writer.h"
#include "src/compat_stub.h"
#include "src/icc-parse.h"
#include "tests/loader/builtin/loader_builtin_memory_test_common.h"
#include "tests/data/color-numeric/vectors.h"
#include "color_test_common.h"

/* ICC s15Fixed16 encoding of the normative sRGB inverse transfer. */
static unsigned int const pc_trc[] = {
    157286u, 62119u, 3417u, 5072u, 2651u, 0u, 0u
};

static unsigned int
pc_read32(unsigned char const *p)
{
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
        ((unsigned int)p[2] << 8) | p[3];
}

static unsigned char const *
pc_tag(unsigned char const *profile, size_t length, char const *name,
       size_t wanted)
{
    size_t i;
    size_t offset;
    size_t size;
    size_t count;

    if (length < 132u || pc_read32(profile) != length ||
        memcmp(profile + 12, "mntrRGB XYZ ", 12u) != 0 ||
        memcmp(profile + 36, "acsp", 4u) != 0) {
        return NULL;
    }
    count = pc_read32(profile + 128);
    if (count > (length - 132u) / 12u) {
        return NULL;
    }
    for (i = 0u; i < count; ++i) {
        if (memcmp(profile + 132u + i * 12u, name, 4u) == 0) {
            offset = pc_read32(profile + 136u + i * 12u);
            size = pc_read32(profile + 140u + i * 12u);
            if (offset > length || size > length - offset || size != wanted) {
                return NULL;
            }
            return profile + offset;
        }
    }
    return NULL;
}

static int
pc_profile(unsigned char const *profile, size_t length)
{
    static char const *const xyz_tags[] = {"rXYZ", "gXYZ", "bXYZ"};
    static char const *const trc_tags[] = {"rTRC", "gTRC", "bTRC"};
    unsigned char const *tag;
    double actual;
    int i;
    int j;

    for (i = 0; i < 3; ++i) {
        tag = pc_tag(profile, length, xyz_tags[i], 20u);
        if (tag == NULL || memcmp(tag, "XYZ ", 4u) != 0) {
            return 0;
        }
        for (j = 0; j < 3; ++j) {
            actual = (double)(int32_t)pc_read32(tag + 8 + j * 4) / 65536.0;
            /* Independent Decimal-derived D50 vectors, not an output from
             * ColorSync, lcms2, or the serializer under test. */
            if (fabs(actual - color_vectors[i][6 + j]) > 0.5 / 65536.0) {
                return 0;
            }
        }
        tag = pc_tag(profile, length, trc_tags[i], 40u);
        if (tag == NULL || memcmp(tag, "para", 4u) != 0 ||
            tag[8] != 0u || tag[9] != 4u) {
            return 0;
        }
        for (j = 0; j < 7; ++j) {
            if (pc_read32(tag + 12 + j * 4) != pc_trc[j]) {
                return 0;
            }
        }
    }
    tag = pc_tag(profile, length, "wtpt", 20u);
    if (tag == NULL || pc_read32(tag + 8) != 63190u ||
        pc_read32(tag + 12) != 65536u || pc_read32(tag + 16) != 54061u) {
        return 0;
    }
    tag = pc_tag(profile, length, "chad", 44u);
    /* Check adaptation on D65, independently of the matrix tag checks. */
    if (tag == NULL || memcmp(tag, "sf32", 4u) != 0) {
        return 0;
    }
    for (i = 0; i < 3; ++i) {
        actual = (double)(int32_t)pc_read32(tag + 8 + i * 12) *
            (0.3127 / 0.3290) +
            (double)(int32_t)pc_read32(tag + 12 + i * 12) +
            (double)(int32_t)pc_read32(tag + 16 + i * 12) *
            ((1.0 - 0.3127 - 0.3290) / 0.3290);
        if (fabs(actual / 65536.0 - color_vectors[3][6 + i]) >
            2.0 / 65536.0) {
            return 0;
        }
    }
    return 1;
}

static int
pc_check(int expected_space, int direct, int sample_case,
         unsigned char const *expected)
{
    unsigned char data[8192];
    unsigned char *pixels;
    unsigned char const *payload;
    sixel_icc_profile_t profile;
    edge_frame_probe_t *probe;
    edge_loader_options_t options;
    SIXELSTATUS status;
    unsigned char rgba[6 * 6 * 4];
    size_t length;
    size_t offset;
    size_t size;
    size_t key;
    int srgb_count;
    int icc_count;
    int saw_pixels;
    int ok;
    int width;
    int height;
    int i;
    int j;

    (void)sixel_compat_set_binary(0);
    length = fread(data, 1u, sizeof(data), stdin);
    if (length < 45u || length == sizeof(data) ||
        memcmp(data, "\211PNG\r\n\032\n", 8u) != 0 ||
        data[24] != 8u || data[25] != (direct == 2 ? 2u : direct ? 6u : 3u)) {
        return EXIT_FAILURE;
    }
    srgb_count = 0;
    icc_count = 0;
    saw_pixels = 0;
    ok = 1;
    for (offset = 8u; offset + 12u <= length; offset += size + 12u) {
        size = pc_read32(data + offset);
        if (size > length - offset - 12u) {
            return EXIT_FAILURE;
        }
        payload = data + offset + 8u;
        if (memcmp(data + offset + 4u, "sRGB", 4u) == 0) {
            ++srgb_count;
            ok &= size == 1u && payload[0] == 1u && !saw_pixels;
        } else if (memcmp(data + offset + 4u, "iCCP", 4u) == 0) {
            ++icc_count;
            key = 0u;
            while (key < size && payload[key] != 0u) {
                ++key;
            }
            if (key + 2u >= size || payload[key + 1u] != 0u) {
                return EXIT_FAILURE;
            }
            memset(&profile, 0, sizeof(profile));
            ok &= !saw_pixels &&
                sixel_icc_parse_png_iccp(data, length, &profile) &&
                profile.kind == SIXEL_ICC_PROFILE_KIND_RGB;
            for (i = 0; i < 3 && ok; ++i) {
                for (j = 0; j < 3; ++j) {
                    ok &= fabs(profile.matrix_to_xyz_d50[j][i] -
                        color_vectors[i][6 + j]) <= 0.5 / 65536.0;
                }
                ok &= profile.curves[i].function_type == 4u &&
                    profile.curves[i].param_count == 7u;
                for (j = 0; j < 7; ++j) {
                    ok &= profile.curves[i].params[j] ==
                        (double)pc_trc[j] / 65536.0;
                }
            }
            sixel_icc_profile_destroy(&profile);
        } else if (memcmp(data + offset + 4u, "PLTE", 4u) == 0 ||
                   memcmp(data + offset + 4u, "IDAT", 4u) == 0) {
            saw_pixels = 1;
        }
    }
    ok &= srgb_count == (expected_space == 0) &&
        icc_count == (expected_space == 1);
    probe = calloc(1u, sizeof(*probe));
    if (probe == NULL) {
        return EXIT_FAILURE;
    }
    edge_loader_options_init(&options);
    options.cms_engine = SIXEL_CMS_ENGINE_NONE;
    options.use_palette = 0;
    options.set_prefer_float32 = 1;
    options.prefer_float32 = 0;
    if (edge_load_buffer_options("PNG writer samples", data, length,
            &options, probe, &status) != 0 || status != SIXEL_OK ||
        probe->callback_count != 1 ||
        probe->pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
        probe->width[0] > 6 || probe->height[0] > 6) {
        free(probe);
        return EXIT_FAILURE;
    }
    width = probe->width[0];
    height = probe->height[0];
    for (i = 0; i < width * height; ++i) {
        memcpy(rgba + i * 4, probe->rgb[0] + i * 3, 3u);
        rgba[i * 4 + 3] = probe->mask_size[0] != 0u &&
            probe->mask[0][i] != 0u ? 0u : 255u;
    }
    free(probe);
    pixels = rgba;
    if (expected != NULL) {
        ok &= width == 3 && height == 1;
        for (i = 0; i < 3 && ok; ++i) {
            ok &= memcmp(pixels + i * 4, expected + i * 3, 3u) == 0 &&
                pixels[i * 4 + 3] == 255u;
        }
    } else if (sample_case >= 2) {
        ok &= width == (sample_case == 2 ? 3 : 6) && height == width;
        if (ok) {
            for (i = 0; i < width * height; ++i) {
                ok &= pixels[i * 4] == 255u &&
                    pixels[i * 4 + 1] == 51u &&
                    pixels[i * 4 + 2] == 0u &&
                    pixels[i * 4 + 3] == 255u;
            }
        }
    } else {
        ok &= width == 3 && height == 1;
    }
    if (ok && expected == NULL && sample_case < 2) {
        ok &= pixels[0] == 255u && pixels[1] == 51u && pixels[2] == 0u &&
            pixels[3] == 255u && pixels[4] == 0u && pixels[5] == 255u &&
            pixels[6] == 102u && pixels[7] == 255u;
        if (direct == 1 && !sample_case) {
            ok &= pixels[11] == 0u;
        }
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
png_color_check(int expected_space, int direct, int sample_case)
{
    return pc_check(expected_space, direct, sample_case, NULL);
}

/* Independent primary matrices/TRCs followed by DEC field rounding give
 * these RGB8 samples. HLS decoding rounds reconstructed RGB to percent
 * before expansion to bytes; the P3 green blue-channel is 29, not 30.
 * The closest HLS field margin is .0176, versus a .002 error budget. */
int
png_snapshot_check(int p3, int hls)
{
    static unsigned char const rgb[3][9] = {
        {255, 0, 0, 0, 255, 0, 0, 0, 255},
        {235, 51, 36, 117, 252, 77, 0, 0, 245},
        {235, 51, 36, 117, 252, 74, 0, 0, 245}
    };

    return pc_check(p3, 0, 0, rgb[p3 ? 1 + hls : 0]);
}

int
png_color_write(char const *path, int mode)
{
    unsigned char pixels[9] = {255, 51, 0, 0, 255, 102, 0, 0, 0};
    unsigned char indices[3] = {0, 1, 2};
    sixel_color_def_t *definition;
    sixel_writer_color_t color;
    SIXELSTATUS status;

    definition = sixel_cdef_name("display-p3");
    color.srgb = 0;
    color.definition = definition;
    if (mode == 0) {
        status = sixel_helper_write_image_file(indices, 3, 1, pixels,
            SIXEL_PIXELFORMAT_PAL8, path, SIXEL_FORMAT_PNG, NULL);
    } else if (mode == 2) {
        status = sixel_write_image_color(pixels, 3, 1, NULL,
            SIXEL_PIXELFORMAT_RGB888, path, SIXEL_FORMAT_PNG, &color, NULL);
    } else {
        status = sixel_write_image_color(indices, 3, 1, pixels,
            SIXEL_PIXELFORMAT_PAL8, path, SIXEL_FORMAT_PNG, &color, NULL);
    }
    sixel_cdef_free(definition);
    return SIXEL_SUCCEEDED(status) ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
png_color_raw(void)
{
    static unsigned char const input[] =
        "\033P0;1q\"1;1;3;1#0;2;100;20;0@#1;2;0;100;40@\033\\";
    sixel_decoder_t *decoder;
    sixel_decode_result_t srgb;
    sixel_decode_result_t p3;
    SIXELSTATUS status;
    int ok;

    memset(&srgb, 0, sizeof(srgb));
    memset(&p3, 0, sizeof(p3));
    decoder = NULL;
    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        return EXIT_FAILURE;
    }
    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT_COLORSPACE,
                                   "srgb");
    ok = SIXEL_SUCCEEDED(status) && SIXEL_SUCCEEDED(
        sixel_decoder_decode_pixels(decoder, input, sizeof(input) - 1u,
                                     NULL, &srgb));
    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT_COLORSPACE,
                                   "display-p3");
    ok &= SIXEL_SUCCEEDED(status) && SIXEL_SUCCEEDED(
        sixel_decoder_decode_pixels(decoder, input, sizeof(input) - 1u,
                                     NULL, &p3));
    if (ok) {
        ok = srgb.width == p3.width && srgb.height == p3.height &&
            srgb.width == 3 && srgb.height == 1 &&
            srgb.stride == p3.stride && srgb.stride >= 12 &&
            srgb.pixelformat == p3.pixelformat &&
            memcmp(srgb.pixels, p3.pixels, 12u) == 0;
    }
    free(srgb.pixels);
    free(p3.pixels);
    sixel_decoder_unref(decoder);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

int
png_color_profile(void)
{
    sixel_color_def_t *definition;
    sixel_allocator_t *allocator;
    unsigned char *bytes;
    size_t length;
    SIXELSTATUS status;
    int ok;

    bytes = NULL;
    definition = sixel_cdef_name("display-p3");
    allocator = NULL;
    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_SUCCEEDED(status)) {
        status = sixel_cdef_export_icc(definition, &bytes, &length, allocator);
    }
    ok = SIXEL_SUCCEEDED(status) && pc_profile(bytes, length);
    free(bytes);
    sixel_allocator_unref(allocator);
    sixel_cdef_free(definition);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
