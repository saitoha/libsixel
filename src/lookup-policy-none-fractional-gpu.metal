#include <metal_stdlib>
using namespace metal;

/* A scalar entry must not assume converged simdgroup participation. Phased
 * invokes lookup on selected pixels; the byte GEMM executor remains separate.
 * Direct squared differences avoid cancellation in |c|^2 - 2 q.c. */
static uint lookup_rgb8_fractional(float3 query, constant uchar *palette,
    uint ncolors, constant uchar *configuration,
    device const uchar *workspace)
{
    uint best, i;
    float minimum, distance;
    float3 gap;

    (void)configuration; (void)workspace;
    best = 0;
    minimum = INFINITY;
    for (i = 0; i < ncolors; ++i) {
        gap = query-float3(palette[3*i],palette[3*i+1],palette[3*i+2]);
        distance = dot(gap, gap);
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    return best;
}
static uint lookup_rgb8(uchar3 query, constant uchar *palette,
    uint ncolors, constant uchar *configuration,
    device const uchar *workspace)
{
    return lookup_rgb8_fractional(float3(query), palette, ncolors,
        configuration, workspace);
}
