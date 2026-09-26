/* SPDX-License-Identifier: MIT */
/* Keep the measurement path independent of bounded storage converters. */
#include <math.h>
#include "color.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int
sixel_qa_xyz(sixel_color_def_t const *srgb, int coordinate,
             double const sample[3], double xyz[3])
{
    double rgb[3];

    /* Working Lab/Oklab/DIN99d describe the existing sRGB D65 basis.
     * Decode that coordinate first, then adapt the physical color to D50. */
    if (coordinate == SIXEL_COLORSPACE_GAMMA) {
        return sixel_cdef_to_xyz(srgb, sample, xyz, 1);
    }
    return sixel_color_convert(rgb, sample, coordinate,
                                SIXEL_COLORSPACE_GAMMA)
        && sixel_cdef_to_xyz(srgb, rgb, xyz, 1);
}

static double
qa_lab_f(double value)
{
    return value > 216.0 / 24389.0 ? cbrt(value)
        : (24389.0 / 27.0 * value + 16.0) / 116.0;
}

void
sixel_qa_lab(double const xyz[3], double lab[3])
{
    double x;
    double y;
    double z;

    x = qa_lab_f(xyz[0] / 0.9642);
    y = qa_lab_f(xyz[1]);
    z = qa_lab_f(xyz[2] / 0.8249);
    lab[0] = 116.0 * y - 16.0;
    lab[1] = 500.0 * (x - y);
    lab[2] = 200.0 * (y - z);
}

/* CIEDE2000, unit parametric factors. Angles are radians except dTheta. */
double
sixel_qa_de00(double const lab1[3], double const lab2[3])
{
    double L1;
    double a1;
    double b1;
    double L2;
    double a2;
    double b2;
    double Lbar;
    double C1;
    double C2;
    double Cbar;
    double G;
    double a1p;
    double a2p;
    double C1p;
    double C2p;
    double h1p;
    double h2p;
    double dLp;
    double dCp;
    double dhp;
    double dHp;
    double Lpm;
    double Cpm;
    double hp_sum;
    double hp_diff;
    double Hpm;
    double T;
    double Sl;
    double Sc;
    double Sh;
    double dTheta;
    double Rc;
    double Rt;
    double de;

    L1 = lab1[0];
    a1 = lab1[1];
    b1 = lab1[2];
    L2 = lab2[0];
    a2 = lab2[1];
    b2 = lab2[2];
    Lbar = 0.5 * (L1 + L2);
    C1 = sqrt(a1 * a1 + b1 * b1);
    C2 = sqrt(a2 * a2 + b2 * b2);
    Cbar = 0.5 * (C1 + C2);
    G = 0.5 * (1.0 - sqrt(pow(Cbar, 7.0) /
                           (pow(Cbar, 7.0) + pow(25.0, 7.0))));
    a1p = (1.0 + G) * a1;
    a2p = (1.0 + G) * a2;
    C1p = sqrt(a1p * a1p + b1 * b1);
    C2p = sqrt(a2p * a2p + b2 * b2);
    /* CIEDE2000 defines zero hue for zero chroma. Avoid atan2(0, 0):
     * runtimes such as OpenVMS return NaN for that undefined angle. */
    h1p = C1p == 0.0 ? 0.0 : atan2(b1, a1p);
    if (h1p < 0.0) {
        h1p += 2.0 * M_PI;
    }
    h2p = C2p == 0.0 ? 0.0 : atan2(b2, a2p);
    if (h2p < 0.0) {
        h2p += 2.0 * M_PI;
    }
    dLp = L2 - L1;
    dCp = C2p - C1p;
    dhp = h2p - h1p;
    if (dhp > M_PI) {
        dhp -= 2.0 * M_PI;
    }
    if (dhp < -M_PI) {
        dhp += 2.0 * M_PI;
    }
    if (C1p * C2p == 0.0) {
        dhp = 0.0;
    }
    dHp = 2.0 * sqrt(C1p * C2p) * sin(dhp / 2.0);
    Lpm = Lbar;
    Cpm = 0.5 * (C1p + C2p);
    hp_sum = h1p + h2p;
    hp_diff = fabs(h1p - h2p);
    if (C1p * C2p == 0.0) {
        Hpm = hp_sum;
    } else {
        if (hp_diff <= M_PI) {
            Hpm = 0.5 * hp_sum;
        } else {
            if (hp_sum < 2.0 * M_PI) {
                Hpm = 0.5 * (hp_sum + 2.0 * M_PI);
            } else {
                Hpm = 0.5 * (hp_sum - 2.0 * M_PI);
            }
        }
    }
    T = 1.0 - 0.17 * cos(Hpm - M_PI / 6.0) +
        0.24 * cos(2.0 * Hpm) +
        0.32 * cos(3.0 * Hpm + M_PI / 30.0) -
        0.20 * cos(4.0 * Hpm - 7.0 * M_PI / 20.0);
    Sl = 1.0 + ((0.015 * pow(Lpm - 50.0, 2.0)) /
                sqrt(20.0 + pow(Lpm - 50.0, 2.0)));
    Sc = 1.0 + 0.045 * Cpm;
    Sh = 1.0 + 0.015 * Cpm * T;
    dTheta = 30.0 * exp(-pow((Hpm - 275.0 * M_PI / 180.0) /
                             (25.0 * M_PI / 180.0), 2.0));
    Rc = 2.0 * sqrt(pow(Cpm, 7.0) /
                    (pow(Cpm, 7.0) + pow(25.0, 7.0)));
    Rt = -Rc * sin(2.0 * dTheta * M_PI / 180.0);
    de = sqrt(pow(dLp / (Sl), 2.0) + pow(dCp / (Sc), 2.0) +
              pow(dHp / (Sh), 2.0) +
              Rt * (dCp / Sc) * (dHp / Sh));
    return de;
}

void
sixel_qa_regions(double const *a, double const *b, int width, int height,
                 int window, double *mean, double *maximum, size_t *count)
{
    int x;
    int y;
    int i;
    int j;
    int c;
    int right;
    int bottom;
    size_t offset;
    size_t area;
    double av[3];
    double bv[3];
    double al[3];
    double bl[3];
    double delta;

    *mean = 0.0;
    *maximum = 0.0;
    *count = 0;
    if (width <= 0 || height <= 0 || window <= 0) {
        return;
    }
    for (y = 0; y < height; y += window) {
        bottom = height - y < window ? height : y + window;
        for (x = 0; x < width; x += window) {
            right = width - x < window ? width : x + window;
            area = (size_t)(right - x) * (size_t)(bottom - y);
            for (c = 0; c < 3; ++c) {
                av[c] = 0.0;
                bv[c] = 0.0;
            }
            for (j = y; j < bottom; ++j) {
                for (i = x; i < right; ++i) {
                    offset = ((size_t)j * (size_t)width + (size_t)i) * 3;
                    for (c = 0; c < 3; ++c) {
                        av[c] += a[offset + c];
                        bv[c] += b[offset + c];
                    }
                }
            }
            for (c = 0; c < 3; ++c) {
                av[c] /= (double)area;
                bv[c] /= (double)area;
            }
            /* Average physical colors before the nonlinear Lab/DE steps. */
            sixel_qa_lab(av, al);
            sixel_qa_lab(bv, bl);
            delta = sixel_qa_de00(al, bl);
            *mean += delta * (double)area;
            if (delta > *maximum) {
                *maximum = delta;
            }
            ++*count;
        }
    }
    *mean /= (double)width * (double)height;
}
