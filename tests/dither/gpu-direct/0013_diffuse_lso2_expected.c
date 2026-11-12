/* SPDX-License-Identifier: MIT */
#include "tests/_support/lso2-gpu/fixture.h"
#include "tests/_support/lso2-gpu/expected.h"

/* A device-less GPU backend (Vulkan) must reproduce the frozen LSO2
 * scatter-oracle indexes that the Metal component reproduces, through
 * both the monolithic run and the deferred solve/resolve. */
enum { DIRECT_LSO2_PLAIN, DIRECT_LSO2_T0, DIRECT_LSO2_REFRESH };

static int
direct_lso2_case(int gain, int history, int deferred,
    unsigned char const *expected)
{
    lso2_gpu_fixture_t f;
    uint32_t selection[128];
    int outcome, x, y, cell;

    outcome = lso2_gpu_fixture_open_direct(&f, SIXEL_DIFFUSE_LSO2);
    if (outcome != 0) goto end;
    outcome = 1;
    f.configuration.options.lso2_gain = gain;
    if (history != DIRECT_LSO2_PLAIN) {
        LSO2_GPU_CHECK(lso2_gpu_fixture_baseline(&f, 0) == SIXEL_OK);
    }
    if (history == DIRECT_LSO2_REFRESH) {
        memset(selection, 0, sizeof(selection));
        for (y = 0; y < 64; ++y) {
            for (x = 0; x < 64; ++x) {
                cell = y * 64 + x;
                if ((x + y) % 2 == 0) {
                    selection[cell / 32] |= 1u << (cell % 32);
                }
            }
        }
        f.run.delta.threshold = 48;
        f.run.delta.selection.columns = f.run.delta.selection.rows = 64;
        f.run.delta.selection.tiled = 1;
        f.run.delta.selection.words = selection;
        f.run.delta.selection.word_count = 128;
    }
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    if (deferred) {
        LSO2_GPU_CHECK(lso2_gpu_fixture_deferred(&f) == SIXEL_OK);
    } else {
        LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    }
    LSO2_GPU_CHECK(memcmp(f.image->indexes, expected, LSO2_GPU_PIXELS) == 0);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}

/* A full 256-entry palette with the reserved transparency entry last. */
static int
direct_lso2_palette256(void)
{
    lso2_gpu_fixture_t f;
    sixel_wire_color_definition_t definitions[256];
    sixel_wire_palette_request_t wire;
    sixel_palette_entries_request_t entries;
    unsigned char colors[768];
    int outcome, x, y, i, j;

    outcome = lso2_gpu_fixture_open_direct(&f, SIXEL_DIFFUSE_LSO2);
    if (outcome != 0) goto end;
    outcome = 1;
    memset(definitions, 0, sizeof(definitions));
    for (i = 0; i < 256; ++i) {
        definitions[i].register_index = (unsigned int)i;
        definitions[i].type = 2;
        definitions[i].fields[0] = (i * 37) % 101;
        definitions[i].fields[1] = (i * 59) % 101;
        definitions[i].fields[2] = (i * 83) % 101;
        for (j = 0; j < 3; ++j) {
            colors[3 * i + j] = (unsigned char)(
                (definitions[i].fields[j] * 255 + 50) / 100);
        }
    }
    entries.entries = colors;
    entries.colors = 256;
    entries.depth = 3;
    LSO2_GPU_CHECK(f.palette->vtbl->init_entries(f.palette, &entries)
        == SIXEL_OK);
    sixel_wire_palette_unref(f.wire);
    f.wire = NULL;
    memset(&wire, 0, sizeof(wire));
    wire.definitions = definitions;
    wire.count = 256;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    LSO2_GPU_CHECK(sixel_wire_palette_new(&f.wire, &wire, f.allocator)
        == SIXEL_OK);
    f.prepare.output.palette = f.wire;
    f.prepare.output.keycolor = 255;
    f.configuration.options.ncolors = 256;
    for (y = 0; y < LSO2_GPU_HEIGHT; ++y) {
        for (x = 0; x < LSO2_GPU_WIDTH; ++x) {
            i = 3 * (y * LSO2_GPU_WIDTH + x);
            f.pixels[i] = (unsigned char)((x * 37 + y * 17) % 256);
            f.pixels[i + 1] = (unsigned char)((x * 11 + y * 43) % 256);
            f.pixels[i + 2] = (unsigned char)((x * 53 + y * 7) % 256);
        }
    }
    LSO2_GPU_CHECK(lso2_gpu_fixture_prepare(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(lso2_gpu_fixture_run(&f) == SIXEL_OK);
    LSO2_GPU_CHECK(memcmp(f.image->indexes, lso2_gpu_expected_palette256,
        LSO2_GPU_PIXELS) == 0);
    outcome = 0;
end:
    lso2_gpu_fixture_close(&f);
    return outcome;
}

int
test_gpu_direct_0013_diffuse_lso2_expected(int argc, char **argv)
{
    int deferred, outcome;

    (void)argc;
    (void)argv;
    for (deferred = 0; deferred < 2; ++deferred) {
        outcome = direct_lso2_case(SIXEL_LSO2_GAIN_STANDARD,
            DIRECT_LSO2_PLAIN, deferred, lso2_gpu_expected_standard);
        if (outcome != 0) return outcome;
        outcome = direct_lso2_case(SIXEL_LSO2_GAIN_SMALL_ERROR,
            DIRECT_LSO2_PLAIN, deferred, lso2_gpu_expected_small_error);
        if (outcome != 0) return outcome;
        outcome = direct_lso2_case(SIXEL_LSO2_GAIN_STANDARD,
            DIRECT_LSO2_T0, deferred, lso2_gpu_expected_t0);
        if (outcome != 0) return outcome;
        outcome = direct_lso2_case(SIXEL_LSO2_GAIN_STANDARD,
            DIRECT_LSO2_REFRESH, deferred, lso2_gpu_expected_refresh);
        if (outcome != 0) return outcome;
    }
    return direct_lso2_palette256();
}
