/* SPDX-License-Identifier: MIT */
#include "tests/_support/phased/fixture.h"
#include "tests/_support/phased/expected.h"

int
test_phased_palette256(int argc, char **argv)
{
    phased_fixture_t f;
    int outcome;
    sixel_wire_color_definition_t definitions[256];
    sixel_wire_palette_request_t wire;
    sixel_palette_entries_request_t entries;
    unsigned char colors[768];
    int x, y, i, j;


    (void)argc;
    (void)argv;
    outcome = phased_fixture_open(&f, 1);
    if (outcome != 0) goto end;
    outcome = 1;
    memset(definitions, 0, sizeof(definitions));
    for (i = 0; i < 256; ++i) {
        definitions[i].register_index = (unsigned int)i;
        definitions[i].type = 2;
        definitions[i].fields[0] = (i*37)%101;
        definitions[i].fields[1] = (i*59)%101;
        definitions[i].fields[2] = (i*83)%101;
        for (j = 0; j < 3; ++j)
            colors[3*i+j] = (unsigned char)(
                (definitions[i].fields[j]*255+50)/100);
    }
    entries.entries = colors;
    entries.colors = 256;
    entries.depth = 3;
    PHASED_CHECK(f.palette->vtbl->init_entries(f.palette, &entries)==SIXEL_OK);
    sixel_wire_palette_unref(f.wire);
    f.wire = NULL;
    memset(&wire, 0, sizeof(wire));
    wire.definitions = definitions;
    wire.count = 256;
    wire.colorspace = SIXEL_COLORSPACE_GAMMA;
    PHASED_CHECK(sixel_wire_palette_new(&f.wire, &wire, f.allocator)
        == SIXEL_OK);
    f.prepare.output.palette = f.wire;
    f.prepare.output.keycolor = 255;
    f.configuration.options.ncolors = 256;
    for (y = 0; y < PHASED_HEIGHT; ++y) {
        for (x = 0; x < PHASED_WIDTH; ++x) {
            i = 3*(y*PHASED_WIDTH+x);
            f.pixels[i] = (unsigned char)((x*37+y*17)%256);
            f.pixels[i+1] = (unsigned char)((x*11+y*43)%256);
            f.pixels[i+2] = (unsigned char)((x*53+y*7)%256);
        }
    }
    PHASED_CHECK(phased_fixture_prepare(&f) == SIXEL_OK);
    PHASED_CHECK(phased_fixture_run(&f) == SIXEL_OK);
    PHASED_CHECK(memcmp(f.image->indexes, phased_expected_palette256,
        PHASED_PIXELS) == 0);
    outcome = 0;
end:
    phased_fixture_close(&f);
    return outcome;
}
