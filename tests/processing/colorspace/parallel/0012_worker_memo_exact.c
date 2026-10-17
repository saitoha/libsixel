/* SPDX-License-Identifier: MIT */
/* Compare growing-worker memo results with the uncached scalar API. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/colorspace.h"
#include "src/threading.h"

static int
worker_memo_12_reclaim(void *context)
{
    int *polls;

    polls = (int *)context;
    return ++*polls == 2;
}

int
test_parallel_float_0012(int argc, char **argv)
{
    static int const spaces[][2] = {
        { SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_CIELAB },
        { SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_OKLAB },
        { SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_DIN99D },
        { SIXEL_COLORSPACE_GAMMA, SIXEL_COLORSPACE_LINEAR }
    };
    float colors[4][3];
    float expected[4][3];
    double input[3];
    double output[3];
    float *pixels;
    sixel_thread_budget_scope_t scope;
    SIXELSTATUS status;
    size_t i;
    size_t count;
    int pair;
    int color;
    int channel;
    int polls;
    int ok;

    (void)argc;
    (void)argv;
    /* The first two exact float triplets map to memo slot 9921. Alternate
     * them across chunk boundaries as well as within a chunk. Both zero
     * signs also recur after distant chunks and must retain their bits. */
    colors[0][0] = 90.0f / 65535.0f;
    colors[1][0] = 150.0f / 65535.0f;
    colors[2][0] = 0.0f;
    colors[3][0] = -0.0f;
    for (color = 0; color < 4; ++color) {
        colors[color][1] = .25f;
        colors[color][2] = .75f;
    }
    /* Seventeen chunks exceed all eight execution slots after 4 -> 8
     * donation. Repeated calls also change the source/target pair. */
    count = 262145U;
    pixels = (float *)malloc(count * 3U * sizeof(float));
    if (pixels == NULL) {
        return EXIT_FAILURE;
    }
    ok = 1;
    for (pair = 0; pair < 4 && ok; ++pair) {
        for (color = 0; color < 4; ++color) {
            for (channel = 0; channel < 3; ++channel) {
                input[channel] = colors[color][channel];
            }
            ok &= sixel_color_convert(output, input,
                spaces[pair][0], spaces[pair][1]);
            for (channel = 0; channel < 3; ++channel) {
                expected[color][channel] = (float)output[channel];
            }
        }
        for (i = 0U; i < count; ++i) {
            color = (int)((i / 16384U + i / 127U) % 4U);
            memcpy(pixels + i * 3U, colors[color], sizeof(colors[0]));
        }
        polls = 0;
        sixel_set_threads(8);
        sixel_thread_budget_enter(&scope, 4);
        sixel_thread_budget_set_reclaim(&scope, 8,
            worker_memo_12_reclaim, &polls);
        status = sixel_color_convert_frame_rgbf32(pixels, count,
            spaces[pair][0], spaces[pair][1]);
        sixel_thread_budget_leave(&scope);
        ok &= polls == 2 && SIXEL_SUCCEEDED(status);
        for (i = 0U; i < count && ok; ++i) {
            color = (int)((i / 16384U + i / 127U) % 4U);
            ok &= memcmp(pixels + i * 3U, expected[color],
                sizeof(expected[0])) == 0;
        }
    }
    free(pixels);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
