/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/dither.h"
#include "src/encoder-core-private.h"
#include "src/gpu-palette.h"

typedef struct lookup_preparation_stream {
    unsigned char data[8192];
    size_t size;
} lookup_preparation_stream_t;

static int
lookup_preparation_write(char *bytes, int size, void *context)
{
    lookup_preparation_stream_t *stream;

    stream = context;
    if (size < 0 || (size_t)size > sizeof(stream->data) - stream->size) {
        return -1;
    }
    memcpy(stream->data + stream->size, bytes, (size_t)size);
    stream->size += (size_t)size;
    return size;
}

/* Observe whether CPU lookup exists after one physical-plan decision.
 * A separately configured CPU encoder checks the resulting complete stream;
 * an empty or accidentally skipped application cannot satisfy this probe. */
int
indexed_gpu_lookup_preparation_test(int mode, int lookup)
{
    sixel_dither_t *dither[2];
    sixel_output_t *output[2];
    lookup_preparation_stream_t streams[2];
    unsigned char palette[33 * 3];
    unsigned char pixels[16 * 16 * 3];
    int i, pass, ok;
    SIXELSTATUS status;

    if (mode < 0 || mode > 5) return 1;
    /* Modes 0 and 1 apply the lookup on the GPU; the device must carry
     * that policy's own program. */
    if (mode <= 1 && !sixel_gpu_palette_lookup_available(lookup)) return 77;
    memset(dither, 0, sizeof(dither));
    memset(output, 0, sizeof(output));
    memset(streams, 0, sizeof(streams));
    ok = 0;
    for (i = 0; i < (int)sizeof(palette); ++i) {
        palette[i] = (unsigned char)(i * 73 + 11);
    }
    for (pass = 0; pass < 2; ++pass) {
        for (i = 0; i < (int)sizeof(pixels); ++i) {
            pixels[i] = (unsigned char)(i * 31);
        }
        if (sixel_dither_new(&dither[pass], 33, NULL) != SIXEL_OK ||
                sixel_output_new(&output[pass], lookup_preparation_write,
                    &streams[pass], NULL) != SIXEL_OK) goto end;
        sixel_dither_set_palette(dither[pass], palette);
        sixel_dither_set_pixelformat(dither[pass], SIXEL_PIXELFORMAT_RGB888);
        /* GPU certlut uses exact certificates, unlike CPU pair-only cells.
         * An explicit palette keeps this comparison at lookup application. */
        sixel_dither_set_lut_policy(dither[pass], pass == 1 && mode <= 1 &&
            lookup == SIXEL_LUT_POLICY_CERTLUT
                ? SIXEL_LUT_POLICY_NONE : lookup);
        if (lookup == SIXEL_LUT_POLICY_FHEDT) {
            dither[pass]->lut_policy_fhedt_use_cache_override = 1;
            dither[pass]->lut_policy_fhedt_use_cache = mode == 5;
        }
        sixel_dither_set_diffusion_type(dither[pass], mode == 4
            ? SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_BLUENOISE_DITHER);
        sixel_dither_set_optimize_palette(dither[pass], 0);
        dither[pass]->gpu_policy = pass == 1 || mode == 3
            ? SIXEL_GPU_POLICY_OFF : mode == 0
            ? SIXEL_GPU_POLICY_FORCE : SIXEL_GPU_POLICY_AUTO;
        dither[pass]->gpu_palette_threshold = mode == 2 ? 1000000U : 0U;
        status = sixel_encode(pixels, 16, 16, 3, dither[pass], output[pass]);
        /* CPU certlut defaults to per-worker mutable instances, so its
         * parallel fallback deliberately keeps the legacy adapter. */
        if (status != SIXEL_OK || (output[pass]->indexed_count != 1 &&
                !(lookup == SIXEL_LUT_POLICY_CERTLUT && mode > 1))) {
            fprintf(stderr, "encode pass %d: status %x, indexed %u\n",
                pass, (unsigned int)status,
                (unsigned int)output[pass]->indexed_count);
            goto end;
        }
    }
    if ((dither[0]->lookup_policy == NULL) != (mode <= 1) ||
            streams[0].size == 0 || streams[0].size != streams[1].size ||
            memcmp(streams[0].data, streams[1].data, streams[0].size)) {
        fprintf(stderr, "lookup presence %d, stream sizes %zu/%zu\n",
            dither[0]->lookup_policy != NULL,
            streams[0].size, streams[1].size);
        goto end;
    }
    ok = 1;
end:
    for (pass = 0; pass < 2; ++pass) {
        sixel_output_unref(output[pass]);
        sixel_dither_unref(dither[pass]);
    }
    return ok ? 0 : 1;
}

int
test_indexed_gpu_lookup_preparation(int argc, char **argv)
{
    if (argc != 2 && argc != 3) return 1;
    return indexed_gpu_lookup_preparation_test(atoi(argv[1]), argc == 3
        ? SIXEL_LUT_POLICY_FHEDT : SIXEL_LUT_POLICY_DELAUNAY);
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
