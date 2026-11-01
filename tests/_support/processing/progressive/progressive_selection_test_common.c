/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/progressive-select.h"
#include "src/palette-factory.h"

#define SELECTION_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "selection case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

static int
selection_test_run(sixel_repair_selection_t *selection, size_t quantum,
                    size_t totals[2])
{
    sixel_repair_work_t work;
    size_t calls;
    int done;

    done = 0;
    totals[0] = totals[1] = 0;
    for (calls = 0; calls < 10000 && !done; ++calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = quantum;
        if (sixel_repair_selection_advance(selection, &work, &done)
                != SIXEL_OK || work.pixel_visits > quantum ||
                work.distance_evaluations > quantum) {
            return 0;
        }
        totals[0] += work.pixel_visits;
        totals[1] += work.distance_evaluations;
    }
    return done;
}

int
test_progressive_selection_case(int mode)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    sixel_repair_selection_t *selection;
    sixel_repair_selection_t *replay;
    sixel_palette_entries_request_t bytes;
    sixel_palette_float32_entries_request_t floats;
    unsigned char black[3] = { 0, 0, 0 };
    float source[30];
    double errors[10];
    unsigned char domain[10];
    size_t count;
    size_t samples;
    unsigned int cap;
    size_t i;
    size_t mass;
    size_t totals[2];
    size_t replay_totals[2];
    int ok;

    allocator = NULL;
    palette = NULL;
    selection = replay = NULL;
    ok = 0;
    count = 10;
    samples = 3;
    cap = 8;
    memset(domain, 1, sizeof(domain));
    for (i = 0; i < count; ++i) {
        source[i * 3] = source[i * 3 + 1] = source[i * 3 + 2] = i / 10.0f;
        errors[i] = 10 - i;
    }
    if (mode == 46 || mode == 47) {
        count = 4;
        errors[0] = 9;
        errors[1] = errors[2] = 4;
        errors[3] = 1;
    } else if (mode == 49) {
        samples = 1;
        for (i = 0; i < count * 3; ++i) {
            source[i] = 1;
        }
        source[1] = source[2] = 0;
    } else if (mode == 50) {
        samples = count;
        for (i = 0; i < count * 3; ++i) {
            source[i] = .5f;
        }
    } else if (mode == 51 || mode == 52) {
        count = samples = 5;
        cap = 3;
        memset(source, 0, sizeof(source));
        source[3] = source[7] = source[11] = 1;
        source[12] = source[13] = source[14] = 1;
    } else if (mode == 53) {
        for (i = 0; i < count; ++i) {
            source[i * 3] = .351f;
            errors[i] = 1e-12;
        }
    }
    SELECTION_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL,
        NULL) == SIXEL_OK);
    SELECTION_REQUIRE(sixel_palette_create_from_factory(&palette,
        allocator) == SIXEL_OK);
    bytes.entries = black;
    bytes.colors = 1;
    bytes.depth = 3;
    floats.entries = source;
    floats.colors = 1;
    floats.depth = 3 * sizeof(float);
    SELECTION_REQUIRE(palette->vtbl->init_entries(palette, &bytes)
        == SIXEL_OK);
    SELECTION_REQUIRE(palette->vtbl->init_entries_float32(palette, &floats)
        == SIXEL_OK);
    SELECTION_REQUIRE(sixel_palette_prepare_output(palette,
        SIXEL_COLORSPACE_GAMMA, SIXEL_PALETTE_OUTPUT_LEGACY,
        SIXEL_COLORSPACE_GAMMA) == SIXEL_OK);
    SELECTION_REQUIRE(sixel_repair_selection_new(&selection, source,
        errors, domain, count, palette, SIXEL_COLORSPACE_GAMMA,
        SIXEL_PALETTETYPE_RGB, 0, .5, mode != 46, samples, cap, allocator)
        == SIXEL_OK);
    SELECTION_REQUIRE(selection_test_run(selection, 1, totals));
    mass = 0;
    for (i = 0; i < selection->sample_count; ++i) {
        mass += selection->samples[i].population;
    }
    SELECTION_REQUIRE(mass == selection->selected_count);
    if (mode == 46 || mode == 47) {
        SELECTION_REQUIRE(selection->selected_count ==
            (size_t)(mode == 46 ? 3 : 4));
        SELECTION_REQUIRE(memcmp(selection->selected, "\1\1\1", 3) == 0);
        SELECTION_REQUIRE(selection->selected[3] == (mode == 47));
    } else if (mode == 48) {
        SELECTION_REQUIRE(selection->sample_count == 3 && mass == 10);
        SELECTION_REQUIRE(selection->samples[0].position == 1 &&
            selection->samples[0].population == 3);
        SELECTION_REQUIRE(selection->samples[1].position == 4 &&
            selection->samples[1].population == 3);
        SELECTION_REQUIRE(selection->samples[2].position == 7 &&
            selection->samples[2].population == 4);
    } else if (mode == 49) {
        SELECTION_REQUIRE(selection->sample_count == 1 && mass == 10);
        SELECTION_REQUIRE(selection->samples[0].position == 4);
        SELECTION_REQUIRE(selection->protected_position == 0 &&
            selection->protected_color == 0 && selection->color_count == 2);
        SELECTION_REQUIRE(selection->colors[0].fields[0] == 100 &&
            selection->colors[0].fields[1] == 0 &&
            selection->colors[1].fields[1] == 100);
    } else if (mode == 50) {
        SELECTION_REQUIRE(selection->color_count == 1 &&
            selection->sample_count == 10);
        for (i = 0; i < count; ++i) {
            SELECTION_REQUIRE(selection->samples[i].error == 10 - i);
        }
    } else if (mode == 51 || mode == 52) {
        /* Black is protected, white is farthest, then red wins the exact
         * red/green/blue distance tie by its first source position. */
        SELECTION_REQUIRE(selection->color_count == 3 &&
            selection->protected_color == 0);
        SELECTION_REQUIRE(memcmp(selection->colors[0].rgb, "\0\0\0", 3)
            == 0);
        SELECTION_REQUIRE(memcmp(selection->colors[1].rgb,
            "\377\377\377", 3) == 0);
        SELECTION_REQUIRE(memcmp(selection->colors[2].rgb,
            "\377\0\0", 3) == 0);
        if (mode == 52) {
            SELECTION_REQUIRE(sixel_repair_selection_new(&replay, source,
                errors, domain, count, palette, SIXEL_COLORSPACE_GAMMA,
                SIXEL_PALETTETYPE_RGB, 0, .5, 1, samples, cap, allocator)
                == SIXEL_OK);
            SELECTION_REQUIRE(selection_test_run(replay, 10000,
                replay_totals));
            SELECTION_REQUIRE(memcmp(totals, replay_totals, sizeof(totals))
                == 0);
            SELECTION_REQUIRE(memcmp(selection->colors, replay->colors,
                selection->color_count * sizeof(*selection->colors)) == 0);
            SELECTION_REQUIRE(memcmp(selection->samples, replay->samples,
                selection->sample_count * sizeof(*selection->samples)) == 0);
        }
    } else if (mode == 53) {
        SELECTION_REQUIRE(selection->protected_color == -1 &&
            selection->protected_position == SIZE_MAX);
    }
    ok = 1;
end:
    sixel_repair_selection_free(selection);
    sixel_repair_selection_free(replay);
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
