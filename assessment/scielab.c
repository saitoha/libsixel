/* SPDX-License-Identifier: MIT */
/*
 * S-CIELAB: opponent filtering followed by CIE76, Zhang and Wandell.
 * Coefficients and sampled Gaussian convention: author's 1998 release,
 * wandell/SCIELAB-1996, revision cacbc977c07a29c3e92a1aec849ee63bba72e6bf.
 * This implementation normalizes each resampled Gaussian to unit DC and
 * repeats symmetric extension for images smaller than the filter support.
 * See docs/quality/metrics.md for the exact reproducibility contract.
 */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "color.h"
#include "scielab.h"

/* CIE1931 2-degree XYZ -> Poirson-Wandell opponent channels.
 * In particular, the RG channel's Z coefficient is positive. */
static double const scielab_opponent[3][3] = {
    { 0.2787336,  0.7218031, -0.1065520},
    {-0.4487736,  0.2898056,  0.0771569},
    { 0.0859513, -0.5899859,  0.5011089}
};

static double const scielab_spreads[3][3] = {
    {0.05, 0.225, 7.0}, {0.0685, 0.826, 0.0}, {0.0920, 0.6451, 0.0}
};
static double const scielab_weights[3][3] = {
    {1.00327, 0.114416, -0.117686},
    {0.616725, 0.383275, 0.0},
    {0.567885, 0.432115, 0.0}
};

/* Form the effective sampled kernel on a >=224 samples/degree grid.
 * Below that density, linear interpolation is represented by convolution
 * with a triangular kernel before downsampling about the exact center.
 * Normalizing AFTER resampling preserves flat colors at every ppd. */
static int
scielab_kernel(double ppd, double spread, double *kernel)
{
    double gaussian[1025];
    double grid;
    double alpha;
    double sum;
    double value;
    int rate;
    int radius;
    int high_radius;
    int i;
    int j;
    int offset;

    rate = ppd < 224.0 ? (int)ceil(224.0 / ppd) : 1;
    grid = ppd * rate;
    high_radius = (int)ceil(grid / 2.0) - 1;
    radius = high_radius / rate;
    alpha = 2.0 * sqrt(log(2.0)) / (spread * grid - 1.0);
    for (i = -high_radius; i <= high_radius; ++i) {
        gaussian[i + high_radius] = exp(-alpha * alpha * i * i);
    }
    sum = 0.0;
    for (i = -radius; i <= radius; ++i) {
        value = 0.0;
        for (j = 1 - rate; j < rate; ++j) {
            offset = i * rate - j;
            if (offset >= -high_radius && offset <= high_radius) {
                value += gaussian[offset + high_radius]
                    * (1.0 - (double)abs(j) / rate);
            }
        }
        kernel[i + radius] = value;
        sum += value;
    }
    for (i = 0; i <= 2 * radius; ++i) {
        kernel[i] /= sum;
    }
    return radius;
}

/* Edge-repeating symmetry: ... b a | a b c | c b ... . The modulo
 * formulation works for one-pixel axes and kernels wider than the image. */
static int
scielab_reflect(int64_t index, int length)
{
    int64_t period;

    if (index >= 0 && index < length) {
        return (int)index;
    }
    period = 2 * (int64_t)length;
    index %= period;
    if (index < 0) {
        index += period;
    }
    return (int)(index < length ? index : period - 1 - index);
}

static void
scielab_filter(double const *xyz, double *filtered, double *plane,
                double *temp, int width, int height, double ppd)
{
    double kernel[1025];
    double value;
    double weight;
    size_t pixels;
    size_t p;
    int channel;
    int component;
    int radius;
    int x;
    int y;
    int k;
    int neighbor;

    pixels = (size_t)width * height;
    memset(filtered, 0, pixels * 3 * sizeof(*filtered));
    for (channel = 0; channel < 3; ++channel) {
        for (p = 0; p < pixels; ++p) {
            plane[p] = scielab_opponent[channel][0] * xyz[3 * p]
                + scielab_opponent[channel][1] * xyz[3 * p + 1]
                + scielab_opponent[channel][2] * xyz[3 * p + 2];
        }
        for (component = 0; component < (channel == 0 ? 3 : 2);
                ++component) {
            radius = scielab_kernel(ppd,
                scielab_spreads[channel][component], kernel);
            weight = scielab_weights[channel][component];
            /* Sum separate 2-D Gaussians, not the outer product of their
             * 1-D sum. Apply the negative surround weight exactly once. */
            for (y = 0; y < height; ++y) {
                for (x = 0; x < width; ++x) {
                    value = 0.0;
                    for (k = -radius; k <= radius; ++k) {
                        neighbor = scielab_reflect((int64_t)x + k, width);
                        value += kernel[k + radius]
                            * plane[(size_t)y * width + neighbor];
                    }
                    temp[(size_t)y * width + x] = value;
                }
            }
            for (y = 0; y < height; ++y) {
                for (x = 0; x < width; ++x) {
                    value = 0.0;
                    for (k = -radius; k <= radius; ++k) {
                        neighbor = scielab_reflect((int64_t)y + k, height);
                        value += kernel[k + radius]
                            * temp[(size_t)neighbor * width + x];
                    }
                    p = (size_t)y * width + x;
                    filtered[3 * p + channel] += weight * value;
                }
            }
        }
    }
}

/* Invert the declared matrix, avoiding a separately rounded inverse. */
static void
scielab_inverse(double inverse[3][3])
{
    double determinant;
    int i;
    int j;

    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            inverse[j][i] =
                scielab_opponent[(i + 1) % 3][(j + 1) % 3]
                * scielab_opponent[(i + 2) % 3][(j + 2) % 3]
                - scielab_opponent[(i + 1) % 3][(j + 2) % 3]
                * scielab_opponent[(i + 2) % 3][(j + 1) % 3];
        }
    }
    determinant = scielab_opponent[0][0] * inverse[0][0]
        + scielab_opponent[0][1] * inverse[1][0]
        + scielab_opponent[0][2] * inverse[2][0];
    for (i = 0; i < 3; ++i) {
        for (j = 0; j < 3; ++j) {
            inverse[i][j] /= determinant;
        }
    }
}

SIXELSTATUS
sixel_qa_scielab(double const *reference, double const *output,
                  int width, int height, double ppd,
                  double *mean, double *maximum, double *map)
{
    double *storage;
    double *ref;
    double *out;
    double *plane;
    double *temp;
    double inverse[3][3];
    double xyz1[3];
    double xyz2[3];
    double lab1[3];
    double lab2[3];
    double delta;
    double difference;
    double total;
    double peak;
    size_t pixels;
    size_t p;
    int i;
    int j;

    if (reference == NULL || output == NULL || mean == NULL
            || maximum == NULL || width <= 0 || height <= 0
            || !isfinite(ppd) || ppd < SIXEL_QA_SCIELAB_PPD_MIN
            || ppd > SIXEL_QA_SCIELAB_PPD_MAX) {
        return SIXEL_BAD_ARGUMENT;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height / (8 * sizeof(double))) {
        return SIXEL_BAD_ALLOCATION;
    }
    pixels = (size_t)width * height;
    for (p = 0; p < 3 * pixels; ++p) {
        if (!isfinite(reference[p]) || !isfinite(output[p])) {
            return SIXEL_BAD_INPUT;
        }
    }
    storage = (double *)malloc(pixels * 8 * sizeof(*storage));
    if (storage == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    ref = storage;
    out = ref + 3 * pixels;
    plane = out + 3 * pixels;
    temp = plane + pixels;
    scielab_filter(reference, ref, plane, temp, width, height, ppd);
    scielab_filter(output, out, plane, temp, width, height, ppd);
    scielab_inverse(inverse);
    total = 0.0;
    peak = 0.0;
    for (p = 0; p < pixels; ++p) {
        for (i = 0; i < 3; ++i) {
            xyz1[i] = 0.0;
            xyz2[i] = 0.0;
            for (j = 0; j < 3; ++j) {
                xyz1[i] += inverse[i][j] * ref[3 * p + j];
                xyz2[i] += inverse[i][j] * out[3 * p + j];
            }
        }
        /* Keep signed filter lobes and extended-gamut colors. Lab's
         * linear branch is defined below zero; clipping changes scores. */
        sixel_qa_lab(xyz1, lab1);
        sixel_qa_lab(xyz2, lab2);
        delta = 0.0;
        for (i = 0; i < 3; ++i) {
            difference = lab1[i] - lab2[i];
            delta += difference * difference;
        }
        delta = sqrt(delta);
        if (!isfinite(delta) || !isfinite(total + delta)) {
            free(storage);
            return SIXEL_BAD_INPUT;
        }
        /* The plane is no longer needed; stage map values so a failed
         * comparison cannot expose a partially updated result map. */
        plane[p] = delta;
        total += delta;
        if (delta > peak) {
            peak = delta;
        }
    }
    if (map != NULL) {
        memcpy(map, plane, pixels * sizeof(*map));
    }
    *mean = total / pixels;
    *maximum = peak;
    free(storage);
    return SIXEL_OK;
}
