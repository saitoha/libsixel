/* SPDX-License-Identifier: MIT */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/progressive-apply.h"

#define APPLICATION_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "application case %d, line %d\n", mode, __LINE__); \
        return EXIT_FAILURE; \
    } \
} while (0)

static int
application_test_run(sixel_repair_application_t *application, size_t quantum,
                      size_t totals[2])
{
    sixel_repair_work_t work;
    size_t calls;
    int done;

    done = 0;
    totals[0] = totals[1] = 0;
    if (sixel_repair_application_begin(application) != SIXEL_OK) {
        return 0;
    }
    for (calls = 0; calls < 10000 && !done; ++calls) {
        memset(&work, 0, sizeof(work));
        work.pixel_limit = work.distance_limit = quantum;
        if (sixel_repair_application_advance(application, &work, &done)
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
test_progressive_application_case(int mode)
{
    sixel_repair_application_t application;
    sixel_palette_emitted_color_t colors[2];
    float source[12];
    double errors[4] = { .3, .3, .1875, 1 };
    unsigned char selected[4] = { 1, 1, 1, 0 };
    unsigned int palette[2] = { 0, 1 };
    unsigned char indices[4];
    unsigned char paint[4];
    unsigned char replay_paint[4];
    unsigned char replay_indices[4];
    double gains[4];
    double replay_gains[4];
    size_t totals[2];
    size_t replay_totals[2];
    size_t i;

    memset(&application, 0, sizeof(application));
    memset(colors, 0, sizeof(colors));
    for (i = 0; i < 3; ++i) {
        colors[1].working[i] = 1;
    }
    for (i = 0; i < 12; ++i) {
        source[i] = .25f;
    }
    application.source = source;
    application.errors = errors;
    application.selected = selected;
    application.colors = colors;
    application.palette = palette;
    application.color_count = 2;
    application.count = 4;
    application.width = 4;
    application.protected_position = SIZE_MAX;
    application.indices = indices;
    application.paint = paint;
    application.gains = gains;
    if (mode >= 55 && mode <= 58) {
        application.strength = 10;
        application.origin_x = 4;
        application.origin_y = 7;
        if (mode == 56) {
            application.protected_position = 0;
        }
    } else if (mode == 59) {
        application.color_count = 0;
    }
    APPLICATION_REQUIRE(application_test_run(&application, 1, totals));
    if (mode == 54 || mode == 56) {
        /* Nearest black reduces .3 to .1875, but the equal-error third
         * pixel keeps, as does the explicitly excluded fourth position. */
        APPLICATION_REQUIRE(memcmp(paint, "\1\1\0\0", 4) == 0);
        APPLICATION_REQUIRE(indices[0] == 0 && indices[1] == 0 &&
            application.painted == 2);
        APPLICATION_REQUIRE(gains[0] == .3 - .1875);
    } else if (mode >= 55 && mode <= 58) {
        /* At (4,7), table taps 171/183 give noise +.3882353: the query
         * chooses white and must reject it against the unperturbed .25.
         * At (5,7), taps 73/153 give -.1137255, so black is accepted. */
        APPLICATION_REQUIRE(memcmp(paint, "\0\1\0\0", 4) == 0);
        APPLICATION_REQUIRE(application.painted == 1);
        if (mode == 57) {
            /* A one-pixel crop uses absolute coordinates, not compact index.
             * Adding a whole noise tile cannot change its decision. */
            application.count = application.width = 1;
            application.origin_x += 64;
            application.origin_y += 64;
            APPLICATION_REQUIRE(application_test_run(&application, 1,
                replay_totals));
            APPLICATION_REQUIRE(paint[0] == 0 && application.painted == 0);
        } else if (mode == 58) {
            application.paint = replay_paint;
            application.indices = replay_indices;
            application.gains = replay_gains;
            APPLICATION_REQUIRE(application_test_run(&application, 10000,
                replay_totals));
            APPLICATION_REQUIRE(memcmp(paint, replay_paint, sizeof(paint))
                == 0);
            APPLICATION_REQUIRE(memcmp(indices, replay_indices,
                sizeof(indices)) == 0);
            APPLICATION_REQUIRE(memcmp(gains, replay_gains, sizeof(gains))
                == 0);
            APPLICATION_REQUIRE(memcmp(totals, replay_totals, sizeof(totals))
                == 0);
        }
    } else {
        APPLICATION_REQUIRE(memcmp(paint, "\0\0\0\0", 4) == 0 &&
            application.painted == 0 && totals[1] == 0);
    }
    return EXIT_SUCCESS;
}
