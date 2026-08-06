/*
 * SPDX-License-Identifier: MIT
 *
 * Pin the gamut-corner anchoring contract.
 *
 * A saturated region worth a fraction of a percent of the frame is always
 * cheaper for a solver to merge away than to keep, and error diffusion cannot
 * recover it once it lies outside the palette hull: at the gamut boundary the
 * diffused error points out of the RGB cube and the clamp discards it.  So the
 * palette is anchored to the eight cube corners after every solver.
 *
 * These cases use an image with no saturated color at all.  Any entry near a
 * corner therefore has to have come from anchoring rather than from the
 * content, which keeps the test independent of how each solver is tuned and of
 * how much of the image the solvers sample.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <6cells.h>
#include <sixel.h>

#include "src/compat_stub.h"
#include "src/dither.h"
#include "src/palette.h"
#include "src/palette-common-cover.h"

#define COVER_WIDTH 64
#define COVER_HEIGHT 64
#define COVER_COLORS 64

/*
 * Content that fills the middle of the cube without ever approaching a corner:
 * every channel sweeps its full inset range, so a solver has plenty of reason
 * to spread its entries, but nothing is saturated.
 *
 * The spread matters.  With content squeezed into a narrow band, even k-center
 * ends up with entries close together, and a merge budget that is far too
 * tight still finds a pair it will accept -- which is exactly how the k-center
 * regression slipped through an earlier version of this test.
 */
static void
cover_fill_inset(unsigned char *pixels)
{
    int const low = 48;
    int const high = 208;
    int x;
    int y;
    size_t offset;

    for (y = 0; y < COVER_HEIGHT; ++y) {
        for (x = 0; x < COVER_WIDTH; ++x) {
            offset = ((size_t)y * COVER_WIDTH + (size_t)x) * 3u;
            pixels[offset] =
                (unsigned char)(low + (x * 5 + y) % (high - low));
            pixels[offset + 1u] =
                (unsigned char)(low + (x + y * 7) % (high - low));
            pixels[offset + 2u] =
                (unsigned char)(low + (x * 11 + y * 3) % (high - low));
        }
    }
}

static unsigned int
cover_distance_sq(unsigned char const *a, unsigned char const *b)
{
    int dr;
    int dg;
    int db;

    dr = (int)a[0] - (int)b[0];
    dg = (int)a[1] - (int)b[1];
    db = (int)a[2] - (int)b[2];

    return (unsigned int)(dr * dr + dg * dg + db * db);
}

static unsigned int
cover_nearest_sq(unsigned char const *rgb,
                 unsigned char const *entries,
                 unsigned int entry_count)
{
    unsigned int best;
    unsigned int index;
    unsigned int candidate;

    best = ~0u;
    for (index = 0u; index < entry_count; ++index) {
        candidate = cover_distance_sq(rgb, entries + (size_t)index * 3u);
        if (candidate < best) {
            best = candidate;
        }
    }

    return best;
}

/* Build a palette for MODEL and copy it out. */
static int
cover_build_palette(int model,
                    unsigned char const *pixels,
                    sixel_allocator_t *allocator,
                    unsigned char *palette_out,
                    unsigned int *ncolors_out)
{
    SIXELSTATUS status;
    sixel_dither_t *dither;
    sixel_palette_entries_view_t view;
    int ok;

    dither = NULL;
    ok = 0;
    memset(&view, 0, sizeof(view));
    *ncolors_out = 0u;

    status = sixel_dither_new(&dither, COVER_COLORS, allocator);
    if (SIXEL_FAILED(status) || dither == NULL) {
        goto end;
    }
    dither->quantize_model = model;
    status = sixel_dither_initialize(dither,
                                     (unsigned char *)pixels,
                                     COVER_WIDTH,
                                     COVER_HEIGHT,
                                     SIXEL_PIXELFORMAT_RGB888,
                                     SIXEL_LARGE_AUTO,
                                     SIXEL_REP_AUTO,
                                     SIXEL_QUALITY_HIGH);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (dither->palette == NULL || dither->palette->vtbl == NULL
            || dither->palette->vtbl->get_entries == NULL) {
        goto end;
    }
    status = dither->palette->vtbl->get_entries(dither->palette, &view);
    if (SIXEL_FAILED(status) || view.entries == NULL || view.depth != 3
            || view.entry_count == 0u) {
        goto end;
    }
    if (view.entry_count > COVER_COLORS) {
        goto end;
    }
    memcpy(palette_out, view.entries, (size_t)view.entry_count * 3u);
    *ncolors_out = view.entry_count;
    ok = 1;

end:
    if (dither != NULL) {
        sixel_dither_unref(dither);
    }
    return ok;
}

/*
 * Count corners the palette reaches.  The tolerance is deliberately loose:
 * what matters is that a corner is reachable, not that the entry sits exactly
 * on it, and a merged anchor can drift.
 */
static unsigned int
cover_count_reached_corners(unsigned char const *palette,
                            unsigned int ncolors)
{
    static unsigned char const corners[8][3] = {
        { 0x00u, 0x00u, 0x00u }, { 0xffu, 0x00u, 0x00u },
        { 0x00u, 0xffu, 0x00u }, { 0x00u, 0x00u, 0xffu },
        { 0xffu, 0xffu, 0x00u }, { 0xffu, 0x00u, 0xffu },
        { 0x00u, 0xffu, 0xffu }, { 0xffu, 0xffu, 0xffu }
    };
    unsigned int reached;
    unsigned int index;

    reached = 0u;
    for (index = 0u; index < 8u; ++index) {
        if (cover_nearest_sq(corners[index], palette, ncolors)
                <= (unsigned int)SIXEL_PALETTE_COVER_NEAR_SQ) {
            reached++;
        }
    }

    return reached;
}

/*
 * Anchoring has to work on a palette whose entries are already spread far
 * apart, because that is what k-center produces by construction: it minimizes
 * the largest distance from any color to its nearest entry, so it leaves no
 * near-duplicate to fund an anchor with.  An earlier version funded anchors
 * only from pairs within a fixed distance, which such a palette never has, and
 * k-center silently went unanchored.  Build that palette directly rather than
 * hoping a solver produces one.
 */
static int
cover_check_spread_palette(void)
{
    unsigned char entries[COVER_COLORS * 3];
    unsigned int index;
    unsigned int reached;
    unsigned int closest;
    unsigned int i;
    unsigned int j;

    /* 4x4x4 lattice inset from the gamut, so every pair is far apart. */
    for (index = 0u; index < COVER_COLORS; ++index) {
        entries[index * 3u] = (unsigned char)(48 + (index & 3u) * 53);
        entries[index * 3u + 1u] = (unsigned char)(48 + ((index >> 2) & 3u) * 53);
        entries[index * 3u + 2u] = (unsigned char)(48 + ((index >> 4) & 3u) * 53);
    }
    closest = ~0u;
    for (i = 0u; i + 1u < COVER_COLORS; ++i) {
        for (j = i + 1u; j < COVER_COLORS; ++j) {
            unsigned int d = cover_distance_sq(entries + (size_t)i * 3u,
                                               entries + (size_t)j * 3u);
            if (d < closest) {
                closest = d;
            }
        }
    }
    if (closest <= (unsigned int)(12 * 12 * 3)) {
        fprintf(stderr,
                "spread fixture is not spread: closest pair %u\n",
                closest);
        return 0;
    }
    if (cover_count_reached_corners(entries, COVER_COLORS) != 0u) {
        fprintf(stderr, "spread fixture already reaches a corner\n");
        return 0;
    }

    if (SIXEL_FAILED(sixel_palette_cover_anchor_rgb888(entries,
                                                       COVER_COLORS,
                                                       3))) {
        fprintf(stderr, "anchoring a spread palette failed\n");
        return 0;
    }
    reached = cover_count_reached_corners(entries, COVER_COLORS);
    if (reached < 8u) {
        fprintf(stderr,
                "spread palette reached only %u of 8 corners; the merge "
                "budget has to scale with the gap it closes\n",
                reached);
        return 0;
    }

    return 1;
}

/*
 * -Q MODEL:cover=on|off reaches the pass through this override, and has to win
 * over the environment: an explicit option is a stronger statement than an
 * inherited variable.
 */
static int
cover_check_override(void)
{
    int ok;

    ok = 0;
    if (sixel_compat_setenv("SIXEL_PALETTE_COVER", "0") != 0) {
        return 0;
    }
    if (sixel_palette_cover_repair_enabled() != 0) {
        fprintf(stderr, "environment did not disable anchoring\n");
        goto end;
    }
    sixel_set_palette_cover_override(1, 1);
    if (sixel_palette_cover_repair_enabled() == 0) {
        fprintf(stderr, "override did not win over the environment\n");
        goto end;
    }
    sixel_set_palette_cover_override(1, 0);
    if (sixel_palette_cover_repair_enabled() != 0) {
        fprintf(stderr, "override could not disable anchoring\n");
        goto end;
    }
    sixel_set_palette_cover_override(0, 1);
    if (sixel_palette_cover_repair_enabled() != 0) {
        fprintf(stderr, "clearing the override did not fall back to env\n");
        goto end;
    }
    ok = 1;

end:
    sixel_set_palette_cover_override(0, 1);
    (void)sixel_compat_setenv("SIXEL_PALETTE_COVER", "1");
    return ok;
}

int
test_palette_0005_cover_anchor(int argc, char **argv)
{
    static int const models[] = {
        SIXEL_QUANTIZE_MODEL_AUTO,
        SIXEL_QUANTIZE_MODEL_MEDIANCUT,
        SIXEL_QUANTIZE_MODEL_KMEANS,
        SIXEL_QUANTIZE_MODEL_KMEDOIDS,
        SIXEL_QUANTIZE_MODEL_KCENTER,
        SIXEL_QUANTIZE_MODEL_STICKY
    };
    static char const *const names[] = {
        "auto", "mediancut", "kmeans", "kmedoids", "kcenter", "sticky"
    };
    sixel_allocator_t *allocator;
    unsigned char *pixels;
    unsigned char palette[COVER_COLORS * 3];
    unsigned int ncolors;
    unsigned int reached;
    unsigned int baseline_colors;
    size_t index;
    int ok;

    (void)argc;
    (void)argv;
    allocator = NULL;
    pixels = NULL;
    ok = 0;

    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return EXIT_FAILURE;
    }
    pixels = (unsigned char *)sixel_allocator_malloc(
        allocator,
        (size_t)COVER_WIDTH * COVER_HEIGHT * 3u);
    if (pixels == NULL) {
        goto end;
    }
    cover_fill_inset(pixels);

    if (!cover_check_spread_palette()) {
        goto end;
    }
    if (!cover_check_override()) {
        goto end;
    }

    for (index = 0u; index < sizeof(models) / sizeof(models[0]); ++index) {
        /*
         * With anchoring off the muted image must not reach the saturated
         * corners: that is what makes the positive case below meaningful.
         */
        if (sixel_compat_setenv("SIXEL_PALETTE_COVER", "0") != 0) {
            fprintf(stderr, "failed to disable cover anchoring\n");
            goto end;
        }
        if (!cover_build_palette(models[index], pixels, allocator,
                                 palette, &ncolors)) {
            fprintf(stderr, "%s: palette build failed\n", names[index]);
            goto end;
        }
        baseline_colors = ncolors;
        reached = cover_count_reached_corners(palette, ncolors);
        if (reached > 2u) {
            fprintf(stderr,
                    "%s: inset image already reached %u corners unanchored\n",
                    names[index],
                    reached);
            goto end;
        }

        if (sixel_compat_setenv("SIXEL_PALETTE_COVER", "1") != 0) {
            fprintf(stderr, "failed to enable cover anchoring\n");
            goto end;
        }
        if (!cover_build_palette(models[index], pixels, allocator,
                                 palette, &ncolors)) {
            fprintf(stderr, "%s: anchored palette build failed\n",
                    names[index]);
            goto end;
        }
        /*
         * Anchors are funded by merging, never by growing the palette: a
         * caller that asked for N colors still gets N.
         */
        if (ncolors != baseline_colors) {
            fprintf(stderr,
                    "%s: anchoring changed the palette size %u -> %u\n",
                    names[index],
                    baseline_colors,
                    ncolors);
            goto end;
        }
        /*
         * Every model has to be anchored.  k-center regressed here once
         * because it spreads its entries by design and so had no cheap pair
         * to fund an anchor with, which a fixed merge budget rejected.
         */
        reached = cover_count_reached_corners(palette, ncolors);
        if (reached < 8u) {
            fprintf(stderr,
                    "%s: only %u of 8 corners reachable after anchoring\n",
                    names[index],
                    reached);
            goto end;
        }
    }
    ok = 1;

end:
    /* Leave anchoring on, which is the default, for whatever runs next. */
    (void)sixel_compat_setenv("SIXEL_PALETTE_COVER", "1");
    if (pixels != NULL) {
        sixel_allocator_free(allocator, pixels);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
