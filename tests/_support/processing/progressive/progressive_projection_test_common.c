/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/palette.h"
#include "src/palette-factory.h"
#include "src/sixel_decode_pixels.h"

#define PROJECTION_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "projection case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

int
test_progressive_projection_case(int mode)
{
    sixel_palette_t *palette;
    sixel_allocator_t *allocator;
    sixel_palette_entries_request_t bytes;
    sixel_palette_float32_entries_request_t floats;
    sixel_palette_emitted_color_t emitted;
    sixel_palette_emitted_color_t saved;
    unsigned char dummy[3] = { 0, 0, 0 };
    float source[3] = { .351f, .499f, 1.25f };
    float original[3];
    unsigned char const *codes;
    unsigned char *decoded;
    unsigned int count;
    unsigned int flags;
    size_t passes;
    char packet[128];
    int expected[3] = { 35, 50, 100 };
    int output_space;
    int definition;
    int workspace;
    int type;
    int width;
    int height;
    int length;
    int channel;
    int ok;

    palette = NULL;
    allocator = NULL;
    decoded = NULL;
    ok = 0;
    workspace = SIXEL_COLORSPACE_GAMMA;
    output_space = SIXEL_COLORSPACE_GAMMA;
    definition = SIXEL_PALETTE_OUTPUT_LEGACY;
    type = SIXEL_PALETTETYPE_RGB;
    if (mode == 41 || mode == 44) {
        source[0] = 1;
        source[1] = source[2] = 0;
        if (mode == 41) {
            type = SIXEL_PALETTETYPE_HLS;
            expected[0] = 120;
            expected[1] = 50;
            expected[2] = 100;
        } else {
            /* sRGB red in Display P3 is approximately
             * (.9175, .2003, .1386), before 101-level quantization. */
            definition = SIXEL_PALETTE_OUTPUT_DISPLAY_P3;
            expected[0] = 92;
            expected[1] = 20;
            expected[2] = 14;
        }
    } else if (mode == 42) {
        /* IEC sRGB .5 decodes to .21404114, hence wire value 21. */
        source[0] = source[1] = source[2] = .5f;
        output_space = SIXEL_COLORSPACE_LINEAR;
        expected[0] = expected[1] = expected[2] = 21;
    } else if (mode == 43) {
        /* The documented SMPTE-C matrix and signed sRGB curve give
         * (-.61283122, 1.12351236, .73120112) for this linear vector. */
        workspace = SIXEL_COLORSPACE_LINEAR;
        output_space = SIXEL_COLORSPACE_SMPTEC;
        source[0] = -.25f;
        source[1] = 1.25f;
        source[2] = .5f;
        expected[0] = 0;
        expected[1] = 100;
        expected[2] = 73;
    }
    memcpy(original, source, sizeof(source));
    PROJECTION_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL,
        NULL) == SIXEL_OK);
    PROJECTION_REQUIRE(sixel_palette_create_from_factory(&palette,
        allocator) == SIXEL_OK);
    bytes.entries = dummy;
    bytes.colors = 1;
    bytes.depth = 3;
    floats.entries = source;
    floats.colors = 1;
    floats.depth = sizeof(source);
    PROJECTION_REQUIRE(palette->vtbl->init_entries(palette, &bytes)
        == SIXEL_OK);
    PROJECTION_REQUIRE(palette->vtbl->init_entries_float32(palette, &floats)
        == SIXEL_OK);
    PROJECTION_REQUIRE(sixel_palette_prepare_output(palette, workspace,
        definition, output_space) == SIXEL_OK);
    memset(&emitted, 0xa5, sizeof(emitted));
    if (mode == 45) {
        saved = emitted;
        source[0] = NAN;
        PROJECTION_REQUIRE(!sixel_palette_project_output(palette, source,
            workspace, type, &emitted));
        PROJECTION_REQUIRE(memcmp(&saved, &emitted, sizeof(saved)) == 0);
        source[0] = INFINITY;
        PROJECTION_REQUIRE(!sixel_palette_project_output(palette, source,
            workspace, type, &emitted));
        PROJECTION_REQUIRE(memcmp(&saved, &emitted, sizeof(saved)) == 0);
        ok = 1;
        goto end;
    }
    PROJECTION_REQUIRE(sixel_palette_project_output(palette, source,
        workspace, type, &emitted));
    PROJECTION_REQUIRE(memcmp(original, source, sizeof(source)) == 0);
    PROJECTION_REQUIRE(emitted.type == (mode == 41 ? 1 : 2));
    PROJECTION_REQUIRE(memcmp(expected, emitted.fields, sizeof(expected))
        == 0);
    PROJECTION_REQUIRE(sixel_palette_get_rgb101(palette, &codes, &count)
        == SIXEL_OK && count == 1);
    if (mode != 41) {
        for (channel = 0; channel < 3; ++channel) {
            PROJECTION_REQUIRE(codes[channel] == emitted.fields[channel]);
        }
    }
    /* Decode the actual fields independently, without color management.
     * History is the receiver's RGB888 in the configured output space. */
    length = snprintf(packet, sizeof(packet),
        "\033P0;1q\"1;1;1;1#0;%d;%d;%d;%d#0@\033\\", emitted.type,
        emitted.fields[0], emitted.fields[1], emitted.fields[2]);
    PROJECTION_REQUIRE(length > 0 && (size_t)length < sizeof(packet));
    PROJECTION_REQUIRE(sixel_decode_sequence_mapped(
        (unsigned char const *)packet, (size_t)length, NULL, &decoded,
        &width, &height, &flags, &passes, allocator) == SIXEL_OK);
    PROJECTION_REQUIRE(width == 1 && height == 1 && passes == 1);
    PROJECTION_REQUIRE(memcmp(decoded, emitted.rgb, 3) == 0 &&
        decoded[3] == 255);
    if (mode == 40 || mode == 41) {
        for (channel = 0; channel < 3; ++channel) {
            PROJECTION_REQUIRE(emitted.working[channel] ==
                (float)(decoded[channel] / 255.0));
        }
    } else if (mode == 42) {
        /* Wire 21 decodes to byte 54, whose linear value is 54/255.
         * Applying the IEC curve yields .49753288 in gamma -W. */
        PROJECTION_REQUIRE(fabs(emitted.working[0] - .49753288) < 2e-7);
    }
    ok = 1;
end:
    sixel_allocator_free(allocator, decoded);
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
