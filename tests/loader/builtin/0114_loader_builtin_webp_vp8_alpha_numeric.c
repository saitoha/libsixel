/* Fix VP8+ALPH RGB bytes and the separate transparency mask. */

#include <stdio.h>
#include <string.h>

#include "loader_builtin_memory_test_common.h"

int
test_loader_0114_loader_builtin_webp_vp8_alpha_numeric(
    int argc,
    char **argv)
{
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    uint64_t rgb_digest;
    uint64_t mask_digest;

    (void)argc;
    (void)argv;
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    rgb_digest = UINT64_C(0);
    mask_digest = UINT64_C(0);
    if (edge_load_fixture(
            "WebP VP8+ALPH",
            "/tests/data/inputs/formats/"
            "webp-vp8-alpha-snake64-alpha00.webp",
            1,
            &probe,
            &status) != 0 || SIXEL_FAILED(status)) {
        return 1;
    }
    if (probe.callback_count != 1 || probe.width[0] != 64 ||
        probe.height[0] != 64 ||
        probe.pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
        probe.rgb_size[0] != 12288u || probe.mask_size[0] != 4096u ||
        probe.alpha_zero_is_transparent[0] == 0) {
        return 1;
    }
    rgb_digest = edge_digest_bytes(probe.rgb[0], probe.rgb_size[0]);
    mask_digest = edge_digest_bytes(probe.mask[0], probe.mask_size[0]);
    if (rgb_digest != UINT64_C(0x7e2856775e2d89f2) ||
        mask_digest != UINT64_C(0x2ac06f32fd6a6b25)) {
        fprintf(stderr,
                "WebP VP8+ALPH digests 0x%016llx 0x%016llx\n",
                (unsigned long long)rgb_digest,
                (unsigned long long)mask_digest);
        return 1;
    }
    return 0;
}
