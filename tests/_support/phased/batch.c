/* SPDX-License-Identifier: MIT */
#include "fixture.h"
#include "src/lookup-policy-gpu.h"

/* Compare the tile ABI with the independently tested exhaustive point ABI.
 * Palettes and inputs are frozen across both runs. The first image block
 * spans the RGB cube, forcing candidate overflow for the 255-color case. */
int
phased_batch_equivalence(unsigned int count, int transparent, int deferred,
    int refresh)
{
    phased_fixture_t f;
    sixel_lookup_policy_configuration_t config;
    sixel_palette_entries_request_t entries;
    sixel_wire_palette_request_t wire;
    sixel_wire_color_definition_t definitions[256];
    sixel_frame_transparency_t alpha;
    unsigned char colors[768], expected[PHASED_PIXELS];
    uint32_t selection[128];
    unsigned int i, c, x, y;
    int outcome;

    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    memset(definitions,0,sizeof(definitions));
    memset(colors,0,sizeof(colors));
    for (i = 0; i < count; ++i) {
        definitions[i].register_index = i;
        definitions[i].type = 2;
        for (c = 0; c < 3; ++c) {
            definitions[i].fields[c] = (i*37+c*29)%101;
            colors[3*i+c] = (unsigned char)
                ((definitions[i].fields[c]*255+50)/100);
        }
    }
    definitions[count].register_index = count;
    definitions[count].type = 2;
    entries.entries = colors; entries.colors = count+1; entries.depth = 3;
    PHASED_CHECK(f.palette->vtbl->init_entries(f.palette,&entries) == SIXEL_OK);
    sixel_wire_palette_unref(f.wire);
    f.wire = NULL;
    memset(&wire,0,sizeof(wire));
    wire.definitions = definitions; wire.count = count+1;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    PHASED_CHECK(sixel_wire_palette_new(&f.wire,&wire,f.allocator) == SIXEL_OK);
    f.prepare.output.palette = f.wire;
    f.prepare.output.keycolor = (int)count;
    f.configuration.options.ncolors = (int)count+1;
    for (i = 0; i < PHASED_PIXELS; ++i) {
        for (c = 0; c < 3; ++c)
            f.pixels[3*i+c] = i < 8 ? (((i >> c)&1) ? 255 : 0)
                : (unsigned char)(i*73+c*31);
    }
    if (transparent) {
        memset(&alpha,0,sizeof(alpha));
        alpha.transparent = -1;
        alpha.transparent_mask_size = PHASED_PIXELS;
        alpha.transparent_mask = sixel_allocator_calloc(f.allocator,
            PHASED_PIXELS,1);
        PHASED_CHECK(alpha.transparent_mask != NULL);
        for (y = 0; y < PHASED_HEIGHT; ++y) {
            for (x = 0; x < PHASED_WIDTH; ++x) {
                alpha.transparent_mask[y*PHASED_WIDTH+x] =
                    transparent == 2 || (x < 8 && y < 8) || (x+y)%5 == 0;
            }
        }
        PHASED_CHECK(f.run.source->vtbl->set_transparency(f.run.source,&alpha)
            == SIXEL_OK);
    }
    if (refresh) {
        PHASED_CHECK(phased_fixture_baseline(&f,0) == SIXEL_OK);
        for (i = 0; i < 128; ++i) selection[i] = 0x55555555U;
        f.run.delta.threshold = 48;
        f.run.delta.selection.columns = f.run.delta.selection.rows = 64;
        f.run.delta.selection.tiled = 1;
        f.run.delta.selection.words = selection;
        f.run.delta.selection.word_count = 128;
    }
    f.lookup->vtbl->unref(f.lookup);
    f.lookup = NULL;
    memset(&config,0,sizeof(config));
    config.kind = SIXEL_LUT_POLICY_NONE;
    config.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    PHASED_CHECK(sixel_gpu_lookup_select(f.allocator,&config,&f.lookup)
        == SIXEL_OK);
    f.prepare.gpu_lookup = f.lookup;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_OK);
    memcpy(expected,f.image->indexes,sizeof(expected));
    f.lookup->vtbl->unref(f.lookup);
    f.lookup = NULL;
    config.kind = SIXEL_LUT_POLICY_BLOCKCULL;
    PHASED_CHECK(sixel_gpu_lookup_select(f.allocator,&config,&f.lookup)
        == SIXEL_OK);
    f.prepare.gpu_lookup = f.lookup;
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK((deferred ? phased_fixture_deferred(&f) :
        phased_fixture_run(&f)) == SIXEL_OK);
    PHASED_CHECK(memcmp(expected,f.image->indexes,sizeof(expected)) == 0);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
