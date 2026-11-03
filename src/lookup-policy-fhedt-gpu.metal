#include <metal_stdlib>
using namespace metal;
#ifndef REFINE
#define REFINE 1
#endif
#if REFINE
typedef ushort Table;
#else
typedef uchar Table;
#endif

#if PACKED
/* A grid cost needs 18 bits and a label needs eight. Reserve UINT_MAX
 * for a seedless cell; actual distances fit below that sentinel. */
typedef uint Grid;
static Grid grid_value(int cost, int label)
{
    return label < 0 ? UINT_MAX : (uint(cost) << 8) | uint(255 - label);
}
static int grid_cost(Grid value) { return int(value >> 8); }
static int grid_label(Grid value)
{
    return value == UINT_MAX ? -1 : 255 - int(value & 255);
}
#else
typedef int2 Grid;
static Grid grid_value(int cost, int label) { return int2(cost, label); }
static int grid_cost(Grid value) { return value.x; }
static int grid_label(Grid value) { return value.y; }
#endif

/* R is specialized once per pipeline compilation.
 * Preserve quantized palette coordinates, collisions and axis ties. */
struct FhedtBuildParams {
    uint colors, pixels, shift, axis, refine, dist2;
};

/* Only R squared column heads need clearing. Seed lists contain at most
 * 256 entries and are consumed after the tracked seed encoder completes. */
kernel void sixel_gpu_fhedt_clear(device uint *heads [[buffer(0)]],
                       uint i [[thread_position_in_grid]])
{
    if (i < R * R) heads[i] = UINT_MAX;
}

kernel void sixel_gpu_fhedt_seed(device atomic_uint *heads [[buffer(0)]],
                      constant uchar *palette [[buffer(2)]],
                      constant FhedtBuildParams &p [[buffer(6)]],
                      uint i [[thread_position_in_grid]])
{
    uint y, z, previous;

    if (i >= p.colors) return;
    y = palette[3 * i + 1] >> p.shift;
    z = palette[3 * i + 2] >> p.shift;
    previous = atomic_exchange_explicit(heads + y + z * R, i,
        memory_order_relaxed);
    atomic_store_explicit(heads + R * R + i, previous,
        memory_order_relaxed);
}

/* A seed's X cost is zero, so scan only that column's palette entries.
 * Threads write consecutive X cells. Explicit tie rules make the result
 * independent of list order: earlier X, then last colliding label. */
kernel void sixel_gpu_fhedt_x(device const uint *heads [[buffer(0)]],
                     device Grid *output [[buffer(1)]],
                     constant uchar *palette [[buffer(2)]],
                     constant FhedtBuildParams &p [[buffer(6)]],
                     uint at [[thread_position_in_grid]])
{
    uint candidate;
    int x, seed, best_x, best, distance, minimum;

    if (at >= R * R * R) return;
    x = int(at % R);
    candidate = heads[at / R];
    minimum = 1 << 28;
    best_x = R;
    best = -1;
    while (candidate != UINT_MAX) {
        seed = palette[3 * candidate] >> p.shift;
        distance = (x - seed) * (x - seed);
        if (distance < minimum || (distance == minimum &&
                (seed < best_x ||
                 (seed == best_x && int(candidate) > best)))) {
            minimum = distance;
            best_x = seed;
            best = int(candidate);
        }
        candidate = heads[R * R + candidate];
    }
    output[at] = grid_value(minimum, best);
}

kernel void sixel_gpu_fhedt_edt(device const Grid *input [[buffer(0)]],
                     device Grid *output [[buffer(1)]],
                     constant FhedtBuildParams &p [[buffer(6)]],
                     device Table *table [[buffer(3)]],
                     uint line [[thread_position_in_grid]])
{
    int sites[R], starts[R];
    int k, q, i, old, numerator, denominator, start, end, at;
    uint base, stride;
    Grid source;
    int cost;
    device ushort *compact;

    if (line >= R * R) return;
    base = p.axis == 0 ? line * R
        : p.axis == 1 ? line % R + (line / R) * R * R : line;
    stride = p.axis == 0 ? 1 : p.axis == 1 ? R : R * R;
    k = -1;
    for (q = 0; q < R; ++q) {
        source = input[base + uint(q) * stride];
        if (grid_label(source) < 0) continue;
        start = -1000000000;
        while (k >= 0) {
            old = sites[k];
            numerator = grid_cost(source) + q * q
                - grid_cost(input[base + uint(old) * stride]) - old * old;
            denominator = 2 * (q - old);
            /* First integer position at which the later source strictly
             * wins. Rounding preserves the CPU earlier-source tie. */
            start = numerator / denominator;
            start -= numerator < 0 && numerator % denominator != 0;
            ++start;
            if (start > starts[k]) break;
            --k;
        }
        ++k;
        sites[k] = q;
        starts[k] = k == 0 ? -1000000000 : start;
    }
    end = k;
    k = 0;
    for (i = 0; i < R; ++i) {
        at = int(base + uint(i) * stride);
        if (end < 0) {
            output[at] = grid_value(1 << 28, -1);
            continue;
        }
        while (k < end && starts[k + 1] <= i) ++k;
        old = sites[k];
        source = input[base + uint(old) * stride];
        cost = grid_cost(source) + (i - old) * (i - old);
        if (p.axis == 2) {
#if REFINE
            /* The final pass no longer needs full distances. Boundary
             * packing only consumes labels and the zero-distance flag. */
            compact = (device ushort *)output;
            compact[at] = ushort(grid_label(source)
                | (uint(cost == 0) << 9));
#else
            table[at] = uchar(grid_label(source));
#endif
            continue;
        }
        output[at] = grid_value(cost, grid_label(source));
    }
}

kernel void sixel_gpu_fhedt_pack(device const ushort *grid [[buffer(0)]],
                      device ushort *table [[buffer(3)]],
                      uint at [[thread_position_in_grid]])
{
    uint x, y, z, boundary;
    int label;

    if (at >= R * R * R) return;
    x = at % R;
    y = (at / R) % R;
    z = at / (R * R);
    label = grid[at] & 255;
    boundary = (x > 0 && (grid[at - 1] & 255) != label)
        || (x + 1 < R && (grid[at + 1] & 255) != label)
        || (y > 0 && (grid[at - R] & 255) != label)
        || (y + 1 < R && (grid[at + R] & 255) != label)
        || (z > 0 && (grid[at - R * R] & 255) != label)
        || (z + 1 < R && (grid[at + R * R] & 255) != label);
    /* Equal axis weights make the optional dist2 shortcut true only at
     * zero distance (the CPU safe radius squared is 0.75 grid units). */
    table[at] = ushort(label | (boundary << 8)
        | (grid[at] & 512));
}
