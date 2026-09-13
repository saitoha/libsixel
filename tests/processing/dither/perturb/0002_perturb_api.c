/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sixel.h>
#include "src/dither.h"

/* Invalid API amounts must not overwrite an existing explicit setting. */
int
test_perturb_0002_api(int argc, char **argv)
{
    sixel_dither_t *dither;
    int failed;

    (void)argc;
    (void)argv;
    dither = NULL;
    if (SIXEL_FAILED(sixel_dither_new(&dither, 2, NULL))) {
        return 1;
    }
    failed = dither->diffusion_perturb != 0.0f
        || dither->diffusion_perturb_override != 0
        || dither->diffusion_perturb_seed != 0
        || dither->diffusion_perturb_seed_override != 0;
    failed |= sixel_dither_set_diffusion_perturb(dither, 0.5f) != SIXEL_OK;
    failed |= sixel_dither_set_diffusion_perturb(dither, -0.1f)
        != SIXEL_BAD_ARGUMENT;
    failed |= sixel_dither_set_diffusion_perturb(dither, 1.5f)
        != SIXEL_BAD_ARGUMENT;
    failed |= sixel_dither_set_diffusion_perturb(dither, NAN)
        != SIXEL_BAD_ARGUMENT;
    failed |= sixel_dither_set_diffusion_perturb(dither, INFINITY)
        != SIXEL_BAD_ARGUMENT;
    failed |= sixel_dither_set_diffusion_perturb(NULL, 0.5f)
        != SIXEL_BAD_ARGUMENT;
    failed |= dither->diffusion_perturb != 0.5f
        || dither->diffusion_perturb_override != 1;
    failed |= sixel_dither_set_diffusion_perturb_seed(dither, -2147483647 - 1)
        != SIXEL_OK;
    failed |= dither->diffusion_perturb_seed != (-2147483647 - 1)
        || dither->diffusion_perturb_seed_override != 1;
    failed |= sixel_dither_set_diffusion_perturb_seed(NULL, 0)
        != SIXEL_BAD_ARGUMENT;
    failed |= sixel_dither_set_diffusion_perturb(dither, 1.0f) != SIXEL_OK;
    failed |= sixel_dither_set_diffusion_perturb(dither, 0.0f) != SIXEL_OK;
    sixel_dither_unref(dither);
    return failed;
}
