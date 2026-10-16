/* SPDX-License-Identifier: MIT */
/* Expose fileno when the test helper is compiled in strict C99 mode. */
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder.h"
#include "src/presentation.h"
#include "src/sixel_decode_pixels.h"
#include "src/compat_stub.h"

#define PRESENT_REQUIRE(value) do { \
    if (!(value)) { \
        fprintf(stderr, "presentation case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

/* Exercise the real file-playback entry point and independently compose its
 * capture. The immutable source and observed bytes have separate owners. */
int
test_progressive_presentation_case(int mode)
{
    sixel_encoder_t *encoder;
    sixel_allocator_t *allocator;
    FILE *capture;
    char path[2048];
    char const *root;
    char const *fixture;
    unsigned char bytes[65536];
    unsigned char *pixels;
    size_t size;
    size_t passes;
    size_t i;
    unsigned int flags;
    uint64_t revision;
    int width;
    int height;
    int ok;
    SIXELSTATUS status;

    encoder = NULL;
    allocator = NULL;
    capture = NULL;
    pixels = NULL;
    ok = 0;
    root = sixel_compat_getenv("TOP_SRCDIR");
    PRESENT_REQUIRE(root != NULL);
    PRESENT_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
        == SIXEL_OK);
    PRESENT_REQUIRE(sixel_encoder_new(&encoder, allocator) == SIXEL_OK);
    PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'Z', "progressive")
        == SIXEL_OK);
    PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'L', "builtin!")
        == SIXEL_OK);
    if (mode == 108) {
        /* Rejected combinations must leave requested and effective policy,
         * fstatic, and all three revisions intact in either option order. */
        revision = encoder->configuration_revision;
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'b', "xterm256")
            != SIXEL_OK);
        PRESENT_REQUIRE(encoder->configuration_revision == revision &&
            encoder->progressive_enabled && !encoder->fstatic);
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'Z', "full")
            == SIXEL_OK && !encoder->progressive_enabled &&
            !encoder->sixdelta_enabled && !encoder->fstatic);
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'b', "xterm256")
            == SIXEL_OK);
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'Z', "progressive")
            != SIXEL_OK && encoder->update_policy == SIXEL_UPDATE_POLICY_FULL);
        ok = 1;
        goto end;
    }
    fixture = "/tests/data/inputs/snake_16.png";
    if (mode == 102 || mode == 103 || mode == 104 || mode == 116) {
        fixture = "/tests/data/inputs/formats/"
            "gif-transparent-anim-dispose3.gif";
    } else if (mode == 105 || mode == 106) {
        fixture = "/tests/data/inputs/formats/"
            "apng_8x8_libpng_default_image_first_valid.png";
    }
    if (mode == 116) {
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'Z', "delta:P1")
            == SIXEL_OK);
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'g', NULL) == SIXEL_OK);
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'l', "disable")
            == SIXEL_OK);
    }
    if (mode == 103) {
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'T', "2") == SIXEL_OK);
    } else if (mode == 104) {
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'T', "-1") == SIXEL_OK);
    } else if (mode == 106) {
        /* No such animation frame exists: the default PNG is not numbered. */
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, 'T', "1") == SIXEL_OK);
    } else if (mode == 107) {
        PRESENT_REQUIRE(sixel_encoder_setopt(encoder, '8', NULL) == SIXEL_OK);
    }
    PRESENT_REQUIRE(sixel_compat_snprintf(path, sizeof(path), "%s%s", root,
        fixture) > 0);
    capture = tmpfile();
    PRESENT_REQUIRE(capture != NULL);
    encoder->outfd = fileno(capture);
    status = sixel_encoder_encode(encoder, path);
    /* Retain tmpfile ownership instead of letting encoder destruction close
     * its borrowed descriptor. No output may escape to stdout. */
    encoder->outfd = 1;
    PRESENT_REQUIRE(fflush(capture) == 0 && fseek(capture, 0, SEEK_SET) == 0);
    size = fread(bytes, 1, sizeof(bytes), capture);
    if (mode == 106) {
        PRESENT_REQUIRE(status != SIXEL_OK && size == 0 &&
            !encoder->accumulation_valid && encoder->progressive_context
                == NULL);
        ok = 1;
        goto end;
    }
    PRESENT_REQUIRE(status == SIXEL_OK && size > 2 &&
        memcmp(bytes, "\0337", 2) == 0 &&
        encoder->progressive_context == NULL && !encoder->fstatic);
    PRESENT_REQUIRE(sixel_decode_sequence_mapped(bytes, size, NULL, &pixels,
        &width, &height, &flags, &passes, allocator) == SIXEL_OK);
    PRESENT_REQUIRE(width == encoder->accumulation_width &&
        height == encoder->accumulation_height && passes > 0);
    for (i = 0; i < (size_t)width * height; ++i) {
        if (pixels[i * 4 + 3] != 0) {
            PRESENT_REQUIRE(memcmp(pixels + i * 4,
                encoder->accumulation_pixels + i * 3, 3) == 0);
        }
    }
    if (mode == 101 || mode == 107) {
        PRESENT_REQUIRE(width == 16 && height == 16 && passes > 1);
    } else if (mode == 105) {
        PRESENT_REQUIRE(width == 8 && height == 8 && passes == 1);
        /* Animation frame zero is blue, whereas the excluded poster is red. */
        PRESENT_REQUIRE(pixels[0] < 10 && pixels[2] > 200);
    } else if (mode == 102) {
        /* Frame zero contains the red foreground, no later blue patch. */
        for (i = 0; i < 64; ++i) {
            PRESENT_REQUIRE(pixels[i * 4 + 3] == 0 || pixels[i * 4 + 2] < 10);
        }
    } else {
        if (mode == 116) {
            /* Three source frames plus final repair, with no idle repair. */
            PRESENT_REQUIRE(passes == 4);
        }
        /* Disposal=3 restores the earlier red image before the blue patch.
         * A raw selected subframe would lose the retained red foreground. */
        PRESENT_REQUIRE(width == 8 && height == 8);
        PRESENT_REQUIRE(pixels[0] == 255 && pixels[3] == 255);
        PRESENT_REQUIRE(pixels[(3 * 8 + 2) * 4 + 2] == 255 &&
            pixels[(3 * 8 + 2) * 4 + 3] == 255);
        PRESENT_REQUIRE(pixels[(7 * 8 + 7) * 4 + 3] == 0);
    }
    ok = 1;
end:
    if (encoder != NULL) {
        encoder->outfd = 1;
    }
    if (capture != NULL) {
        fclose(capture);
    }
    sixel_allocator_free(allocator, pixels);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}
