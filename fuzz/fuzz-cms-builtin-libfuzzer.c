/* SPDX-License-Identifier: MIT */

#include <stddef.h>
#include <stdint.h>

#include "src/icc-apply.h"
#include "src/icc-parse.h"

int
LLVMFuzzerTestOneInput(uint8_t const *data, size_t size)
{
    static double const samples[][4] = {
        {0.125, 0.375, 0.625, 0.75},
        {-0.25, 1.25, 0.3125, 0.5}
    };
    static double const reverse_xyz[] = {0.25, 0.375, 0.5};
    sixel_icc_profile_t profile;
    double xyz[3];
    double device[4];
    size_t channels;
    size_t sample;
    unsigned int slot;

    /* Feed ICC bytes directly so mutations reach the tag and stage grammar
     * without having to preserve an image container or compressed stream. */
    if (size > 1024u * 1024u ||
        !sixel_icc_parse_profile(data, size, &profile)) {
        return 0;
    }

    channels = 3u;
    if (profile.kind == SIXEL_ICC_PROFILE_KIND_GRAY) {
        channels = 1u;
    } else if (profile.kind == SIXEL_ICC_PROFILE_KIND_CMYK) {
        channels = 4u;
    }
    for (slot = 0u; slot < SIXEL_ICC_MPET_SLOT_COUNT; ++slot) {
        /* Even a reverse-only profile can evaluate independent PCS input. */
        (void)sixel_icc_apply_physical_xyz_to_device(
            device, channels, reverse_xyz, &profile, slot);
        for (sample = 0u; sample < sizeof(samples) / sizeof(samples[0]);
             ++sample) {
            if (sixel_icc_apply_device_to_physical_xyz(
                    xyz, samples[sample], channels, &profile, slot)) {
                (void)sixel_icc_apply_physical_xyz_to_device(
                    device, channels, xyz, &profile, slot);
            }
        }
    }
    (void)sixel_icc_apply_device_to_xyz_d50_with_a2b_slot(
        xyz, samples[0], channels, &profile, SIXEL_ICC_MPET_SLOT_COUNT, 1);
    sixel_icc_profile_destroy(&profile);
    return 0;
}
