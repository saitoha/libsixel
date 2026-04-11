/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 */

#if HAVE_CONFIG_H
# include "config.h"
#endif

#include <string.h>

#include <sixel.h>

#include "frombmp.h"
#include "frombmp-internal.h"

SIXELSTATUS
sixel_frombmp_probe(
    sixel_chunk_t const *chunk,
    sixel_frombmp_probe_t *probe,
    int info40_mode)
{
    SIXELSTATUS status;
    sixel_bmp_decode_info_t info;

    status = SIXEL_FALSE;
    memset(&info, 0, sizeof(info));
    if (chunk == NULL || probe == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * Probe parses only BMP container metadata and returns payload/profile
     * views without decoding pixel rows.
     */
    memset(probe, 0, sizeof(*probe));
    status = sixel_bmp_parse_header(chunk, &info, info40_mode);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    probe->width = info.width;
    probe->height = info.height;
    probe->bpp = info.bpp;
    probe->is_cmyk = info.is_cmyk;
    probe->dib_family = info.dib_family;
    probe->compression = info.compression;
    probe->payload = info.payload;
    probe->payload_size = info.payload_size;
    probe->icc_profile = info.icc_profile;
    probe->icc_profile_length = info.icc_profile_length;
    probe->has_calibrated_rgb = info.has_calibrated_rgb;
    probe->calibrated_gamma = info.calibrated_gamma;
    probe->calibrated_gamma_r = info.calibrated_gamma_r;
    probe->calibrated_gamma_g = info.calibrated_gamma_g;
    probe->calibrated_gamma_b = info.calibrated_gamma_b;
    probe->white_x = info.white_x;
    probe->white_y = info.white_y;
    probe->red_x = info.red_x;
    probe->red_y = info.red_y;
    probe->green_x = info.green_x;
    probe->green_y = info.green_y;
    probe->blue_x = info.blue_x;
    probe->blue_y = info.blue_y;

    return SIXEL_OK;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
