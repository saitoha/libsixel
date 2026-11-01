/*
 * SPDX-License-Identifier: MIT
 *
 * Compare every vector band-word helper with the scalar update it replaces,
 * on random words, registers, rows, coverage and sixel bytes. Fixed inputs
 * can hide lane bugs that only show for particular byte values, so each
 * helper is exercised many times with independent random data.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/decoder-columns.h"
#include "src/decoder-ormode.h"
#include "src/decoder-simd.h"

#if SIXEL_DECODER_NEON
static unsigned long g_state = 12345u;

static unsigned int
next_random(void)
{
    g_state = g_state * 1103515245u + 12345u;
    return (unsigned int)(g_state >> 16) & 0x7fffu;
}

static uint64_t
random_word(void)
{
    uint64_t word;
    int i;

    word = 0;
    for (i = 0; i < 8; ++i) {
        word = (word << 8) | (uint64_t)(next_random() & 0xffu);
    }
    return word;
}

/* OR blocks: accept only sixteen sixels and match word |= bits * spread. */
static int
check_planes(void)
{
    uint64_t expect[16];
    uint64_t actual[16];
    unsigned char bytes[16];
    uint64_t spread;
    int rows;
    int last;
    int expect_rows;
    int expect_last;
    int bad;
    int i;

    spread = sixel_ormode_spread(next_random() & 255u);
    bad = (int)(next_random() % 24u);
    for (i = 0; i < 16; ++i) {
        expect[i] = actual[i] = random_word();
        bytes[i] = (unsigned char)('?' + (next_random() & 63u));
    }
    if (bad < 16) {
        bytes[bad] = (unsigned char)"$-#!0\x1b>\x7f"[next_random() % 8u];
    }
    rows = 0;
    last = 7;
    if (!sixel_simd_planes_or16(actual, spread, bytes, &rows, &last)) {
        if (bad >= 16 || memcmp(expect, actual, sizeof(expect)) != 0) {
            fprintf(stderr, "planes: rejected valid block or wrote\n");
            return 0;
        }
        return 1;
    }
    if (bad < 16) {
        fprintf(stderr, "planes: accepted byte %d\n", bytes[bad]);
        return 0;
    }
    expect_rows = 0;
    expect_last = -1;
    for (i = 0; i < 16; ++i) {
        sixel_ormode_store(expect + i, spread, bytes[i] - '?', 1);
        if (bytes[i] != '?') {
            expect_last = i;
            expect_rows |= bytes[i] - '?';
        }
    }
    if (memcmp(expect, actual, sizeof(expect)) != 0 ||
            rows != expect_rows || last != expect_last) {
        fprintf(stderr, "planes: words or extent differ\n");
        return 0;
    }
    return 1;
}

/* OR gather: one row index per word, matching sixel_ormode_gather(). */
static int
check_gather(void)
{
    uint64_t words[16];
    unsigned char actual[16];
    int row;
    int i;

    row = (int)(next_random() % 6u);
    for (i = 0; i < 16; ++i) {
        words[i] = random_word();
    }
    vst1q_u8(actual, sixel_simd_gather16(words, row));
    for (i = 0; i < 16; ++i) {
        if (actual[i] != sixel_ormode_gather(words[i], row)) {
            fprintf(stderr, "gather: row %d word %d differs\n", row, i);
            return 0;
        }
    }
    return 1;
}

/* Ordinary rows: byte r is the index, bit r of byte six is coverage. */
static int
check_rows(void)
{
    uint64_t words[16];
    unsigned char index[16];
    unsigned char painted[16];
    uint8x16_t mask;
    int row;
    int i;

    row = (int)(next_random() % 6u);
    for (i = 0; i < 16; ++i) {
        words[i] = random_word();
    }
    vst1q_u8(index, sixel_simd_columns_row16(words, row, &mask));
    vst1q_u8(painted, mask);
    for (i = 0; i < 16; ++i) {
        if (index[i] != (unsigned char)(words[i] >> (row * 8)) ||
                painted[i] != (((words[i] >> (48 + row)) & 1u) ? 255 : 0)) {
            fprintf(stderr, "rows: row %d word %d differs\n", row, i);
            return 0;
        }
    }
    return 1;
}

/* RGBA blend: painted lanes take the color, others keep the destination,
 * including the all-painted shortcut. */
static int
check_blend(void)
{
    unsigned char expect[64];
    unsigned char actual[64];
    unsigned char lanes[16];
    uint32_t colors[16];
    int all;
    int i;

    all = (next_random() & 3u) == 0;
    for (i = 0; i < 16; ++i) {
        colors[i] = (uint32_t)random_word();
        lanes[i] = (unsigned char)(all || (next_random() & 1u) ? 255 : 0);
    }
    for (i = 0; i < 64; ++i) {
        expect[i] = actual[i] = (unsigned char)next_random();
    }
    for (i = 0; i < 16; ++i) {
        if (lanes[i] != 0) {
            memcpy(expect + i * 4, &colors[i], 4);
        }
    }
    sixel_simd_blend_rgba16(actual, colors, vld1q_u8(lanes));
    if (memcmp(expect, actual, sizeof(expect)) != 0) {
        fprintf(stderr, "blend: pixels differ\n");
        return 0;
    }
    return 1;
}
#endif /* SIXEL_DECODER_NEON */

int
test_decoder_0104(int argc, char **argv)
{
#if SIXEL_DECODER_NEON
    int i;

    for (i = 0; i < 20000; ++i) {
        if (!check_planes() || !check_gather() || !check_rows() ||
                !check_blend()) {
            return EXIT_FAILURE;
        }
    }
#endif
    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
