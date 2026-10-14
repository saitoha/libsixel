/* SPDX-License-Identifier: MIT
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Binary search with local MINDE, CSS Color 4, 2026-10-09 section 14.2.2.
 * Search in Cartesian Oklab along a constant-L, constant-hue ray. Clipping
 * is evaluated in the destination's linear device domain, BEFORE any ICC
 * inverse curve can hide out-of-gamut values. Monotone channel transfers
 * make this equivalent to clipping encoded destination RGB.
 */
#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif
#include <math.h>
#include <string.h>
#include "gamut.h"
#include "colorspace.h"

typedef struct sixel_gamut_vtbl {
    int (*apply)(sixel_gamut_t const *, double const[3], double[3], int);
    void (*destroy)(sixel_gamut_t *);
} sixel_gamut_vtbl_t;

struct sixel_gamut {
    sixel_gamut_vtbl_t const *vtbl;
    sixel_allocator_t *allocator;
    double forward[9];
    double backward[9];
    double minimum[3];
    double maximum[3];
};

static void
sixel_gamut_matrix(double const matrix[9], double const in[3], double out[3])
{
    int i;

    for (i = 0; i < 3; ++i) {
        out[i] = matrix[i * 3] * in[0] + matrix[i * 3 + 1] * in[1]
            + matrix[i * 3 + 2] * in[2];
    }
}

static int
sixel_gamut_contains(sixel_gamut_t const *mapper, double const device[3])
{
    int i;

    /* Float frame stores and the Oklab matrix pair can straddle a boundary.
     * This tolerance is numerical only, not an additional visible gamut. */
    for (i = 0; i < 3; ++i) {
        if (!isfinite(device[i]) ||
                device[i] < mapper->minimum[i] - 2e-7 ||
                device[i] > mapper->maximum[i] + 2e-7) {
            return 0;
        }
    }
    return 1;
}

static int
sixel_gamut_clip(sixel_gamut_t const *mapper, double const lab[3],
                  double clipped[3], double *delta)
{
    double linear[3];
    double device[3];
    double result[3];
    double difference;
    int i;

    if (!sixel_color_convert(linear, lab, SIXEL_COLORSPACE_OKLAB,
            SIXEL_COLORSPACE_LINEAR)) {
        return 0;
    }
    sixel_gamut_matrix(mapper->forward, linear, device);
    for (i = 0; i < 3; ++i) {
        if (!isfinite(device[i])) {
            return 0;
        }
        device[i] = fmax(mapper->minimum[i],
            fmin(mapper->maximum[i], device[i]));
    }
    sixel_gamut_matrix(mapper->backward, device, clipped);
    if (!sixel_color_convert(result, clipped, SIXEL_COLORSPACE_LINEAR,
            SIXEL_COLORSPACE_OKLAB)) {
        return 0;
    }
    *delta = 0.0;
    for (i = 0; i < 3; ++i) {
        difference = result[i] - lab[i];
        *delta = hypot(*delta, difference);
    }
    return isfinite(*delta);
}

static int
sixel_gamut_local_minde(sixel_gamut_t const *mapper, double const input[3],
                        double output[3], int colorspace)
{
    double linear[3];
    double device[3];
    double lab[3];
    double candidate[3];
    double clipped[3];
    double result[3];
    double chroma;
    double low;
    double high;
    double middle;
    double delta;
    int low_inside;
    int iteration;

    if (!sixel_color_convert(linear, input, colorspace,
            SIXEL_COLORSPACE_LINEAR)) {
        return 0;
    }
    sixel_gamut_matrix(mapper->forward, linear, device);
    if (sixel_gamut_contains(mapper, device)) {
        /* Identity must not acquire a conversion round trip. */
        memcpy(output, input, 3u * sizeof(double));
        return 1;
    }
    if (!sixel_color_convert(lab, linear, SIXEL_COLORSPACE_LINEAR,
            SIXEL_COLORSPACE_OKLAB)) {
        return 0;
    }
    if (lab[0] >= 1.0 || lab[0] <= 0.0) {
        sixel_gamut_matrix(mapper->backward,
            lab[0] >= 1.0 ? mapper->maximum : mapper->minimum, clipped);
    } else {
        if (!sixel_gamut_clip(mapper, lab, clipped, &delta)) {
            return 0;
        }
        chroma = hypot(lab[1], lab[2]);
        low = 0.0;
        high = chroma;
        low_inside = 1;
        /* Bounded even for finite, extreme extended coordinates. The
         * ordinary CSS epsilon stop needs far fewer than 128 iterations. */
        for (iteration = 0; delta >= 0.02 && high - low > 0.0001 &&
                iteration < 128; ++iteration) {
            middle = low + (high - low) * 0.5;
            candidate[0] = lab[0];
            candidate[1] = lab[1] * (middle / chroma);
            candidate[2] = lab[2] * (middle / chroma);
            if (!sixel_color_convert(linear, candidate,
                    SIXEL_COLORSPACE_OKLAB, SIXEL_COLORSPACE_LINEAR)) {
                return 0;
            }
            sixel_gamut_matrix(mapper->forward, linear, device);
            if (low_inside && sixel_gamut_contains(mapper, device)) {
                low = middle;
                continue;
            }
            if (!sixel_gamut_clip(mapper, candidate, clipped, &delta)) {
                return 0;
            }
            if (delta < 0.02) {
                if (0.02 - delta < 0.0001) {
                    break;
                }
                low_inside = 0;
                low = middle;
                /* Keep searching up to JND, not the first smaller error. */
                delta = 0.02;
            } else {
                high = middle;
            }
        }
    }
    if (!sixel_color_convert(result, clipped, SIXEL_COLORSPACE_LINEAR,
            colorspace)) {
        return 0;
    }
    memcpy(output, result, sizeof(result));
    return 1;
}

static void
sixel_gamut_destroy(sixel_gamut_t *mapper)
{
    sixel_allocator_t *allocator;

    allocator = mapper->allocator;
    sixel_allocator_free(allocator, mapper);
    sixel_allocator_unref(allocator);
}

static sixel_gamut_vtbl_t const sixel_gamut_local_minde_vtbl = {
    sixel_gamut_local_minde, sixel_gamut_destroy
};

SIXELSTATUS
sixel_gamut_new(sixel_gamut_t **out, sixel_color_def_t const *destination,
                 int legacy_colorspace, sixel_allocator_t *allocator)
{
    sixel_gamut_t *mapper;
    sixel_color_def_t *srgb;
    sixel_color_def_t *source;
    sixel_color_def_t *target;
    double basis[3];
    double xyz[3];
    double forward[3];
    double backward[3];
    double zero[3] = {0.0, 0.0, 0.0};
    double one[3] = {1.0, 1.0, 1.0};
    int i;
    int j;
    int ok;

    if (out == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *out = NULL;
    srgb = sixel_cdef_name("srgb");
    source = sixel_cdef_linearized(srgb);
    target = sixel_cdef_linearized(destination != NULL ? destination : srgb);
    mapper = sixel_allocator_calloc(allocator, 1u, sizeof(*mapper));
    ok = source != NULL && target != NULL && mapper != NULL;
    if (ok) {
        memcpy(mapper->maximum, one, sizeof(one));
        if (destination != NULL) {
            ok = sixel_cdef_transfer(destination, zero, mapper->minimum, 0)
                && sixel_cdef_transfer(destination, one, mapper->maximum, 0)
                && sixel_cdef_output_ready(destination, 1, 1);
        }
    }
    for (i = 0; ok && i < 3; ++i) {
        ok = mapper->minimum[i] < mapper->maximum[i];
        memcpy(basis, zero, sizeof(basis));
        basis[i] = 1.0;
        if (destination == NULL && legacy_colorspace ==
                SIXEL_COLORSPACE_SMPTEC) {
            ok = ok && sixel_color_convert_output(forward, basis,
                SIXEL_COLORSPACE_LINEAR, SIXEL_COLORSPACE_SMPTEC)
                && sixel_color_from_output(backward, basis,
                    SIXEL_COLORSPACE_SMPTEC, SIXEL_COLORSPACE_LINEAR);
            for (j = 0; ok && j < 3; ++j) {
                forward[j] = copysign(pow(fabs(forward[j]), 2.2),
                    forward[j]);
            }
        } else {
            ok = ok && sixel_cdef_to_xyz(source, basis, xyz, 1)
                && sixel_cdef_from_xyz(target, xyz, forward, 1)
                && sixel_cdef_to_xyz(target, basis, xyz, 1)
                && sixel_cdef_from_xyz(source, xyz, backward, 1);
        }
        for (j = 0; ok && j < 3; ++j) {
            mapper->forward[j * 3 + i] = forward[j];
            mapper->backward[j * 3 + i] = backward[j];
        }
    }
    sixel_cdef_free(target);
    sixel_cdef_free(source);
    sixel_cdef_free(srgb);
    if (!ok) {
        if (mapper == NULL) {
            return SIXEL_BAD_ALLOCATION;
        }
        sixel_allocator_free(allocator, mapper);
        sixel_helper_set_additional_message(
            "local-minde requires a supported matrix/TRC RGB gamut.");
        return SIXEL_BAD_ARGUMENT;
    }
    mapper->vtbl = &sixel_gamut_local_minde_vtbl;
    mapper->allocator = allocator;
    sixel_allocator_ref(allocator);
    *out = mapper;
    return SIXEL_OK;
}

int
sixel_gamut_apply(sixel_gamut_t const *mapper, double const input[3],
                   double output[3], int colorspace)
{
    return mapper != NULL && input != NULL && output != NULL &&
        mapper->vtbl->apply(mapper, input, output, colorspace);
}

void
sixel_gamut_free(sixel_gamut_t *mapper)
{
    if (mapper != NULL) {
        mapper->vtbl->destroy(mapper);
    }
}
