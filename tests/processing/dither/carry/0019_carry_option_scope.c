/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <sixel.h>
#include "src/encoder.h"
int
test_carry_0019(int argc, char **argv)
{
    static char const *const accepted[] = {
        "auto:deadzone=3.25", "fs:D3.25", "atkinson:D3.25",
        "jajuni:D3.25", "stucki:D3.25", "burkes:D3.25",
        "sierra:variant=1:D3.25", "sierra:variant=2:D3.25",
        "sierra:variant=3:D3.25"
    };
    static char const *const rejected[] = {
        "fs:deadzone=-1", "fs:deadzone=nan", "fs:deadzone=inf",
        "fs:deadzone=1e999", "none:deadzone=5", "lso2:deadzone=5",
        "interframe:deadzone=5", "stbn:deadzone=5", "bluenoise:deadzone=5"
    };
    sixel_encoder_t *encoder;
    SIXELSTATUS status;
    size_t i;
    int ok;

    (void)argc;
    (void)argv;
    encoder = NULL;
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL))) return 1;
    ok = encoder->diffusion_deadzone == 5.0;
    for (i = 0; i < sizeof(accepted) / sizeof(accepted[0]); ++i) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
                                      accepted[i]);
        if (SIXEL_FAILED(status) || encoder->diffusion_deadzone != 3.25
                || encoder->diffusion_deadzone_override != 1) ok = 0;
    }
    for (i = 0; i < sizeof(rejected) / sizeof(rejected[0]); ++i) {
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
                                      rejected[i]);
        if (SIXEL_SUCCEEDED(status)) ok = 0;
    }
    status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "fs");
    if (SIXEL_FAILED(status) || encoder->diffusion_deadzone != 5.0
            || encoder->diffusion_deadzone_override != 0) ok = 0;
    sixel_encoder_unref(encoder);
    return ok ? 0 : 1;
}
