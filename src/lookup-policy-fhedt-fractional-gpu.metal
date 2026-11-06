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
struct FhedtFractionalParams {
    uint shift, refine, dist2, unused;
};
#define FH_HEAD_WORDS (R*R+256)
#define FH_CELLS (R*R*R)
#define FH_TABLE_WORD (FH_HEAD_WORDS+2*FH_CELLS)

/* Only R squared column heads need clearing. Seed lists contain at most
 * 256 entries and are consumed after the tracked seed encoder completes. */
kernel void fhedt_clear(device uint *heads [[buffer(3)]],
                       uint i [[thread_position_in_grid]])
{
    if (i < R * R) heads[i] = UINT_MAX;
}

kernel void fhedt_seed(device atomic_uint *heads [[buffer(3)]],
                      constant uchar *palette [[buffer(0)]],
                      constant uint &count [[buffer(1)]],
                      constant FhedtFractionalParams &p [[buffer(2)]],
                      uint i [[thread_position_in_grid]])
{
    uint y, z, previous;

    if (i >= count) return;
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
kernel void fhedt_x(device uint *heads [[buffer(3)]],
                     constant uchar *palette [[buffer(0)]],
                     constant FhedtFractionalParams &p [[buffer(2)]],
                     uint at [[thread_position_in_grid]])
{
    device Grid *output;
    uint candidate;
    int x, seed, best_x, best, distance, minimum;

    if (at >= R * R * R) return;
    output = (device Grid *)(heads+FH_HEAD_WORDS);
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

static void fhedt_edt(device const Grid *input, device Grid *output,
    device Table *table, uint axis, uint line)
{
    int sites[R], starts[R];
    int k, q, i, old, numerator, denominator, start, end, at;
    uint base, stride;
    Grid source;
    int cost;
    device ushort *compact;

    if (line >= R * R) return;
    base = axis == 0 ? line * R
        : axis == 1 ? line % R + (line / R) * R * R : line;
    stride = axis == 0 ? 1 : axis == 1 ? R : R * R;
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
        if (axis == 2) {
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

kernel void fhedt_pack(device uint *workspace [[buffer(3)]],
                      uint at [[thread_position_in_grid]])
{
    device const ushort *grid;
    device ushort *table;
    uint x, y, z, boundary;
    int label;

    if (at >= R * R * R) return;
    grid = (device const ushort *)(workspace+FH_HEAD_WORDS);
    table = (device ushort *)(workspace+FH_TABLE_WORD);
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
    /* Retain the zero-distance bit; the fractional query also verifies
     * coincidence with the grid seed before using this shortcut. */
    table[at] = ushort(label | (boundary << 8)
        | (grid[at] & 512));
}

/* The axis passes read and write disjoint regions in one leased allocation.
 * Encoder boundaries provide visibility between construction passes. */
kernel void fhedt_y(device uint *w [[buffer(3)]],
    uint line [[thread_position_in_grid]])
{
    fhedt_edt(w+FH_HEAD_WORDS, w+FH_HEAD_WORDS+FH_CELLS,
        (device Table *)(w+FH_TABLE_WORD), 1, line);
}
kernel void fhedt_z(device uint *w [[buffer(3)]],
    uint line [[thread_position_in_grid]])
{
    fhedt_edt(w+FH_HEAD_WORDS+FH_CELLS, w+FH_HEAD_WORDS,
        (device Table *)(w+FH_TABLE_WORD), 2, line);
}

static uint lookup_rgb8_fractional(float3 query, constant uchar *palette,
    uint count, constant uchar *configuration,
    device const uchar *workspace)
{
    constant FhedtFractionalParams &p =
        *(constant FhedtFractionalParams *)configuration;
    device const uchar *bytes;
    device const ushort *table;
    uint3 cell, corner;
    uint at, word, best, candidate, x, y, z;
    int3 color;
    float3 gap;
    float distance, minimum;

    (void)count;
    cell = uint3(query) >> p.shift;
    at = cell.x + cell.y*R + cell.z*R*R;
    bytes = workspace+FH_TABLE_WORD*4;
    if (!p.refine) return bytes[at];
    table = (device const ushort *)bytes;
    word = table[at];
    best = word & 255u;
    /* A zero-cost grid point cannot certify the fractional space around it.
     * Apply the shortcut only when the actual query is that seed point. */
    if (!(word & 256u) || (p.dist2 && (word & 512u) &&
            all(query == float3(cell << p.shift)))) return best;
    minimum = INFINITY;
    for (z = 0; z < 2; ++z) {
        for (y = 0; y < 2; ++y) {
            for (x = 0; x < 2; ++x) {
                corner = min(cell+uint3(x,y,z), uint3(R-1));
                at = corner.x + corner.y*R + corner.z*R*R;
                candidate = table[at] & 255u;
                color = int3(palette[3*candidate], palette[3*candidate+1],
                    palette[3*candidate+2]);
                color = (color >> p.shift) << p.shift;
                gap = query-float3(color);
                distance = dot(gap, gap);
                /* Keep the original corner traversal's tie order. */
                if (distance < minimum) {
                    minimum = distance; best = candidate;
                }
            }
        }
    }
    return best;
}
static uint lookup_rgb8(uchar3 query, constant uchar *palette, uint count,
    constant uchar *configuration, device const uchar *workspace)
{
    return lookup_rgb8_fractional(float3(query), palette, count,
        configuration, workspace);
}
