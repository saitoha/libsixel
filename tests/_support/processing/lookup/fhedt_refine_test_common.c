/* SPDX-License-Identifier: MIT */
/* Compare lookup with the original ordered, deduplicating corner algorithm.
 * Read grid labels through an unrefined handle: no private layout or nearest
 * palette oracle substitutes for the approximate grid's actual candidates. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sixel.h>
#include "src/lookup-fhedt-8bit.h"
#include "src/lookup-fhedt-float32.h"

struct fh_refine_fixture {
    sixel_lookup_fhedt_8bit_t *byte_grid;
    sixel_lookup_fhedt_8bit_t *byte_lookup;
    sixel_lookup_fhedt_float32_t *float_grid;
    sixel_lookup_fhedt_float32_t *float_lookup;
    unsigned char bytes[257 * 3];
    float floats[257 * 3];
    int seeds[257][3];
    int next[257];
    int weights[3];
    int resolution;
    int floating;
};

static int
fh_refine_label(struct fh_refine_fixture *f, int const *point)
{
    unsigned char bytes[3];
    float floats[3];
    int axis;

    for (axis = 0; axis < 3; ++axis) {
        bytes[axis] = (unsigned char)(point[axis] * (256 / f->resolution));
        /* The float palette extends the nominal RGB box to [-.25, 1.25].
         * Lattice-site rounding is well inside its half-cell margin. */
        floats[axis] = (float)(-.25 + 1.5 * point[axis] /
                              (f->resolution - 1));
    }
    return f->floating ?
        sixel_lookup_fhedt_float32_map(f->float_grid, floats) :
        sixel_lookup_fhedt_8bit_map(f->byte_grid, bytes);
}

static int
fh_refine_reference(struct fh_refine_fixture *f,
                    unsigned char const *bytes, float const *floats)
{
    int lower[3];
    int site[3];
    int point[3];
    int used[8];
    int used_count;
    int axis;
    int direction;
    int boundary;
    int index;
    int candidate;
    int cx;
    int cy;
    int cz;
    int idx;
    int best_index;
    double scaled;
    double dx;
    double dy;
    double dz;
    double distance;
    double best_distance;

    for (axis = 0; axis < 3; ++axis) {
        if (f->floating) {
            scaled = ((double)floats[axis] + .25) * (1.0 / 1.5)
                   * (f->resolution - 1);
            lower[axis] = (int)scaled;
            site[axis] = (int)(scaled + .5);
        } else {
            lower[axis] = site[axis] = bytes[axis] / (256 / f->resolution);
        }
    }
    index = fh_refine_label(f, site);
    boundary = 0;
    for (axis = 0; axis < 3; ++axis) {
        for (direction = -1; direction <= 1; direction += 2) {
            memcpy(point, site, sizeof(point));
            point[axis] += direction;
            if (point[axis] >= 0 && point[axis] < f->resolution &&
                    fh_refine_label(f, point) != index) {
                boundary = 1;
            }
        }
    }
    if (!boundary && (!f->floating || f->next[index] < 0)) {
        return index;
    }

    /* This is the pre-optimization refinement: visit Z/Y/X corners, reject
     * every repeated head, then follow a float seed chain in stored order.
     * The expression and strict comparison retain its exact tie semantics. */
    used_count = 0;
    best_index = -1;
    best_distance = DBL_MAX;
    for (cz = 0; cz < 2; ++cz) {
        for (cy = 0; cy < 2; ++cy) {
            for (cx = 0; cx < 2; ++cx) {
                point[0] = lower[0] + cx;
                point[1] = lower[1] + cy;
                point[2] = lower[2] + cz;
                for (axis = 0; axis < 3; ++axis) {
                    if (point[axis] >= f->resolution) {
                        point[axis] = f->resolution - 1;
                    }
                }
                candidate = fh_refine_label(f, point);
                for (idx = 0; idx < used_count; ++idx) {
                    if (used[idx] == candidate) {
                        candidate = -1;
                        break;
                    }
                }
                if (candidate < 0) {
                    continue;
                }
                used[used_count++] = candidate;
                do {
                    if (f->floating) {
                        dx = (double)floats[0] -
                             (double)f->floats[candidate * 3];
                        dy = (double)floats[1] -
                             (double)f->floats[candidate * 3 + 1];
                        dz = (double)floats[2] -
                             (double)f->floats[candidate * 3 + 2];
                    } else {
                        dx = bytes[0] - f->seeds[candidate][0] *
                             (256 / f->resolution);
                        dy = bytes[1] - f->seeds[candidate][1] *
                             (256 / f->resolution);
                        dz = bytes[2] - f->seeds[candidate][2] *
                             (256 / f->resolution);
                    }
                    distance = (double)f->weights[0] * dx * dx
                             + (double)f->weights[1] * dy * dy
                             + (double)f->weights[2] * dz * dz;
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_index = candidate;
                    }
                    candidate = f->floating ? f->next[candidate] : -1;
                } while (candidate >= 0);
            }
        }
    }
    return best_index;
}

static int
fh_refine_setup(struct fh_refine_fixture *f, int weighted)
{
    uint32_t state;
    int index;
    int previous;
    int axis;
    int value;
    int count;
    SIXELSTATUS status;

    count = weighted ? 257 : 256;
    f->weights[0] = weighted ? 7 : 1;
    f->weights[1] = weighted ? 3 : 1;
    f->weights[2] = 1;
    state = 1741;
    for (index = 0; index < 257; ++index) {
        for (axis = 0; axis < 3; ++axis) {
            state = state * 1664525U + 1013904223U;
            value = (int)(state >> 24);
            if (index < 2) {
                value = index * 255;
            }
            /* A colliding group with distinct native coordinates exercises
             * chain order; duplicate pairs elsewhere exercise exact ties. */
            if (index >= 240) {
                value = 128;
            }
            f->bytes[index * 3 + axis] = (unsigned char)value;
            f->floats[index * 3 + axis] = (float)(-.25 + 1.5 * value / 255);
            if (index >= 240) {
                f->floats[index * 3 + axis] += (index % 3) * .00001f;
            }
            if (index > 2 && index < 240 && index % 7 == 0) {
                f->bytes[index * 3 + axis] = f->bytes[(index - 1) * 3 + axis];
                f->floats[index * 3 + axis] =
                    f->floats[(index - 1) * 3 + axis];
            }
            f->seeds[index][axis] = f->floating ?
                (int)(((double)f->floats[index * 3 + axis] + .25) *
                      (1.0 / 1.5) * (f->resolution - 1) + .5) :
                f->bytes[index * 3 + axis] / (256 / f->resolution);
        }
        f->next[index] = -1;
        for (previous = index - 1; previous >= 0; --previous) {
            if (memcmp(f->seeds[index], f->seeds[previous],
                       sizeof(f->seeds[index])) == 0) {
                f->next[index] = previous;
                break;
            }
        }
    }
    if (f->floating) {
        status = sixel_lookup_fhedt_float32_configure(f->float_grid,
            f->floats, count, f->resolution, 0, 0, 0, 0, 8, 8, 0, 0,
            (float)f->weights[0], (float)f->weights[1], 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, 0);
        if (SIXEL_FAILED(status)) {
            return 0;
        }
        status = sixel_lookup_fhedt_float32_configure(f->float_lookup,
            f->floats, count, f->resolution, 1, 0, 0, 0, 8, 8, 0, 0,
            (float)f->weights[0], (float)f->weights[1], 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, 0);
    } else {
        status = sixel_lookup_fhedt_8bit_configure(f->byte_grid,
            f->bytes, count, f->resolution, 0, 0, 0, 0, 8, 8, 0, 0,
            f->weights[0], f->weights[1], 1, SIXEL_PIXELFORMAT_RGB888, 3, 0);
        if (SIXEL_FAILED(status)) {
            return 0;
        }
        status = sixel_lookup_fhedt_8bit_configure(f->byte_lookup,
            f->bytes, count, f->resolution, 1, 0, 0, 0, 8, 8, 0, 0,
            f->weights[0], f->weights[1], 1, SIXEL_PIXELFORMAT_RGB888, 3, 0);
    }
    return SIXEL_SUCCEEDED(status);
}

int
test_fh_refine_common(int floating, int resolution)
{
    struct fh_refine_fixture f;
    sixel_allocator_t *allocator;
    unsigned char bytes[3];
    float floats[3];
    uint32_t state;
    int sample;
    int samples;
    int axis;
    int coord;
    int actual;
    int expected;
    int weighted;
    int result;

    memset(&f, 0, sizeof(f));
    allocator = NULL;
    result = 1;
    f.floating = floating;
    f.resolution = resolution;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL))) {
        return 1;
    }
    if (SIXEL_FAILED(sixel_lookup_fhedt_8bit_create(allocator, &f.byte_grid)) ||
        SIXEL_FAILED(sixel_lookup_fhedt_8bit_create(allocator,
                                                   &f.byte_lookup)) ||
        SIXEL_FAILED(sixel_lookup_fhedt_float32_create(allocator,
                                                     &f.float_grid)) ||
        SIXEL_FAILED(sixel_lookup_fhedt_float32_create(allocator,
                                                     &f.float_lookup))) {
        goto end;
    }
    samples = resolution == 64 ? 64 * 64 * 64 : 65536;
    for (weighted = 0; weighted < 2; ++weighted) {
        if (!fh_refine_setup(&f, weighted)) {
            goto end;
        }
        state = 981;
        for (sample = 0; sample < samples + 8; ++sample) {
            for (axis = 0; axis < 3; ++axis) {
                state = state * 1664525U + 1013904223U;
                coord = resolution == 64 ?
                    (sample >> (axis * 6)) & 63 :
                    (int)((state >> 16) % (unsigned)resolution);
                if (sample >= samples) {
                    coord = (sample & (1 << axis)) ? resolution - 1 : 0;
                }
                bytes[axis] = (unsigned char)(coord * (256 / resolution) +
                    ((state >> 24) % (unsigned)(256 / resolution)));
                floats[axis] = (float)(-.25 + 1.5 *
                    (coord + .125 + .75 * (state >> 24) / 256) /
                    (resolution - 1));
                if (sample >= samples) {
                    floats[axis] = coord ? 1.25f : -.25f;
                }
                if (floats[axis] > 1.25f) {
                    floats[axis] = 1.25f;
                }
            }
            expected = fh_refine_reference(&f, bytes, floats);
            actual = floating ?
                sixel_lookup_fhedt_float32_map(f.float_lookup, floats) :
                sixel_lookup_fhedt_8bit_map(f.byte_lookup, bytes);
            if (actual != expected) {
                fprintf(stderr, "refine float=%d R=%d weight=%d sample=%d: "
                        "%d != %d\n", floating, resolution, weighted,
                        sample, actual, expected);
                goto end;
            }
        }
    }
    result = 0;
end:
    sixel_lookup_fhedt_8bit_unref(f.byte_grid);
    sixel_lookup_fhedt_8bit_unref(f.byte_lookup);
    sixel_lookup_fhedt_float32_unref(f.float_grid);
    sixel_lookup_fhedt_float32_unref(f.float_lookup);
    sixel_allocator_unref(allocator);
    return result;
}
