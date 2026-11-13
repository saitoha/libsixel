/* SPDX-License-Identifier: MIT */
/*
 * FHEDT lookup in the "vulkan.lookup-rgb8.v1" program ABI. The builders
 * port the table build of lookup-policy-fhedt-gpu.metal; the queries keep
 * the CPU quantized-coordinate and corner tie order of
 * fhedt_palette_lookup() and its fractional form.
 *
 * Constants (words): 0 quantization shift, 1 refine, 2 dist2 shortcut,
 * 3 resolution R, 4 word offset of the finished table, 5 compact byte labels.
 *
 * The workspace is uint words: column heads [0, R*R), one seed link per
 * palette entry at R*R + seed, then grids A and B of R^3 cells. Grid
 * values pack cost << 8 | (255 - label), UINT_MAX for a seedless cell.
 * Passes: clear heads, seed lists, X scan (heads -> A), Y (A -> B),
 * Z (B -> A) and, with refinement, pack (A -> B). The Z pass leaves
 * label | zero-distance << 9 words; pack adds the boundary bit 8. The
 * finished table is A without refinement and B with it. Byte R256 adds
 * a final compaction pass from A words to B bytes.
 *
 * vulkan-builders: fhedt_clear fhedt_seed fhedt_x fhedt_y fhedt_z fhedt_pack fhedt_compact
 */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer FhedtWorkspace {
    uint fhedt_words[];
};

/* Byte R256 queries need only a label. The final pass writes one byte per
 * cell in the inactive grid; fractional queries retain word metadata. */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer FhedtByteWorkspace {
    uint8_t fhedt_bytes[];
};

const uint FHEDT_NONE = 0xffffffffu;

uint fhedt_r()
{
    return lookup_constants[3];
}

uint fhedt_grid_a()
{
    return fhedt_r() * fhedt_r() + 256u;
}

uint fhedt_grid_b()
{
    return fhedt_grid_a() + fhedt_r() * fhedt_r() * fhedt_r();
}

#if defined(SIXEL_LOOKUP_BUILDER)
uint fhedt_index()
{
    return gl_GlobalInvocationID.y * gl_NumWorkGroups.x *
        gl_WorkGroupSize.x + gl_GlobalInvocationID.x;
}

uint fhedt_grid_value(int cost, int label)
{
    return label < 0 ? FHEDT_NONE : (uint(cost) << 8) | uint(255 - label);
}

int fhedt_grid_cost(uint value)
{
    return int(value >> 8);
}

int fhedt_grid_label(uint value)
{
    return value == FHEDT_NONE ? -1 : 255 - int(value & 255u);
}

/* Only R squared column heads need clearing. */
void fhedt_clear()
{
    uint i = fhedt_index();

    if (i < fhedt_r() * fhedt_r()) {
        fhedt_words[i] = FHEDT_NONE;
    }
}

/* Each seed pushes itself onto its (y, z) column list. */
void fhedt_seed()
{
    uint i, y, z, previous;

    i = fhedt_index();
    if (i >= lookup_count) {
        return;
    }
    y = uint(lookup_palette[3u * i + 1u]) >> lookup_constants[0];
    z = uint(lookup_palette[3u * i + 2u]) >> lookup_constants[0];
    previous = atomicExchange(fhedt_words[y + z * fhedt_r()], i);
    fhedt_words[fhedt_r() * fhedt_r() + i] = previous;
}

/* A seed's X cost is zero, so scan only that column's palette entries.
 * Explicit tie rules make the result independent of list order: earlier
 * X, then last colliding label. */
void fhedt_x()
{
    uint at, r, candidate;
    int x, seed, best_x, best, cost, minimum;

    at = fhedt_index();
    r = fhedt_r();
    if (at >= r * r * r) {
        return;
    }
    x = int(at % r);
    candidate = fhedt_words[at / r];
    minimum = 1 << 28;
    best_x = int(r);
    best = -1;
    while (candidate != FHEDT_NONE) {
        seed = int(uint(lookup_palette[3u * candidate]) >>
            lookup_constants[0]);
        cost = (x - seed) * (x - seed);
        if (cost < minimum || (cost == minimum &&
                (seed < best_x ||
                 (seed == best_x && int(candidate) > best)))) {
            minimum = cost;
            best_x = seed;
            best = int(candidate);
        }
        candidate = fhedt_words[r * r + candidate];
    }
    fhedt_words[fhedt_grid_a() + at] = fhedt_grid_value(minimum, best);
}

/* floor(numerator / denominator) for a positive denominator, without
 * relying on signed division or remainder rounding. */
int fhedt_floor_div(int numerator, int denominator)
{
    if (numerator >= 0) {
        return numerator / denominator;
    }
    return -((-numerator + denominator - 1) / denominator);
}

/* One lower-envelope pass along Y (axis 1) or Z (axis 2) from grid input
 * to grid output. The Z pass writes the label and zero-distance flag
 * instead of a cost. */
void fhedt_edt(uint axis, uint from, uint to)
{
    int sites[256];
    int starts[256];
    int k, q, i, old, numerator, start, end, r;
    uint line, base, stride, at, source;
    int cost;

    line = fhedt_index();
    r = int(fhedt_r());
    if (line >= uint(r * r)) {
        return;
    }
    base = axis == 1u ? line % uint(r) + (line / uint(r)) * uint(r * r)
        : line;
    stride = axis == 1u ? uint(r) : uint(r * r);
    k = -1;
    start = 0;
    for (q = 0; q < r; ++q) {
        source = fhedt_words[from + base + uint(q) * stride];
        if (fhedt_grid_label(source) < 0) continue;
        start = -1000000000;
        while (k >= 0) {
            old = sites[k];
            numerator = fhedt_grid_cost(source) + q * q
                - fhedt_grid_cost(fhedt_words[from + base +
                    uint(old) * stride])
                - old * old;
            /* First integer position at which the later source strictly
             * wins. Flooring preserves the CPU earlier-source tie. */
            start = fhedt_floor_div(numerator, 2 * (q - old)) + 1;
            if (start > starts[k]) break;
            --k;
        }
        ++k;
        sites[k] = q;
        starts[k] = k == 0 ? -1000000000 : start;
    }
    end = k;
    k = 0;
    for (i = 0; i < r; ++i) {
        at = to + base + uint(i) * stride;
        if (end < 0) {
            fhedt_words[at] = fhedt_grid_value(1 << 28, -1);
            continue;
        }
        while (k < end && starts[k + 1] <= i) ++k;
        old = sites[k];
        source = fhedt_words[from + base + uint(old) * stride];
        cost = fhedt_grid_cost(source) + (i - old) * (i - old);
        if (axis == 2u) {
            fhedt_words[at] = uint(fhedt_grid_label(source)) |
                (uint(cost == 0) << 9);
            continue;
        }
        fhedt_words[at] = fhedt_grid_value(cost, fhedt_grid_label(source));
    }
}

void fhedt_y()
{
    fhedt_edt(1u, fhedt_grid_a(), fhedt_grid_b());
}

void fhedt_z()
{
    fhedt_edt(2u, fhedt_grid_b(), fhedt_grid_a());
}

void fhedt_compact()
{
    uint at = fhedt_index();
    uint cells = fhedt_r() * fhedt_r() * fhedt_r();
    if (at < cells)
        fhedt_bytes[4u * fhedt_grid_b() + at] =
            uint8_t(fhedt_words[fhedt_grid_a() + at] & 255u);
}

/* Boundary packing only consumes labels and the zero-distance flag. */
void fhedt_pack()
{
    uint at, x, y, z, boundary, label, r, from;

    at = fhedt_index();
    r = fhedt_r();
    if (at >= r * r * r) {
        return;
    }
    from = fhedt_grid_a();
    x = at % r;
    y = (at / r) % r;
    z = at / (r * r);
    label = fhedt_words[from + at] & 255u;
    boundary = ((x > 0u && (fhedt_words[from + at - 1u] & 255u) != label)
        || (x + 1u < r && (fhedt_words[from + at + 1u] & 255u) != label)
        || (y > 0u && (fhedt_words[from + at - r] & 255u) != label)
        || (y + 1u < r && (fhedt_words[from + at + r] & 255u) != label)
        || (z > 0u && (fhedt_words[from + at - r * r] & 255u) != label)
        || (z + 1u < r &&
            (fhedt_words[from + at + r * r] & 255u) != label))
        ? 1u : 0u;
    /* Equal axis weights make the optional dist2 shortcut true only at
     * zero distance (the CPU safe radius squared is 0.75 grid units). */
    fhedt_words[fhedt_grid_b() + at] = label | (boundary << 8) |
        (fhedt_words[from + at] & 512u);
}
#endif

uint fhedt_label(uint at)
{
    if (lookup_constants[5] != 0u)
        return uint(fhedt_bytes[4u * lookup_constants[4] + at]);
    return fhedt_words[lookup_constants[4] + at];
}

/* Byte FHEDT: the query's cell, then the corner refinement for a boundary
 * cell, as fhedt_palette_lookup() in lookup-policy-fhedt-gpu-query.metal. */
uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint r, at, word, best, minimum, dist2, candidate, x, y, z;
    uvec3 cell, corner;
    ivec3 d, color;

    r = fhedt_r();
    cell = query >> lookup_constants[0];
    at = cell.x + cell.y * r + cell.z * r * r;
    word = fhedt_label(at);
    best = word & 255u;
    if (lookup_constants[1] == 0u || (word & 256u) == 0u ||
            (lookup_constants[2] != 0u && (word & 512u) != 0u)) {
        return best;
    }
    minimum = 0xffffffffu;
    for (z = 0u; z < 2u; ++z) {
        for (y = 0u; y < 2u; ++y) {
            for (x = 0u; x < 2u; ++x) {
                corner = min(cell + uvec3(x, y, z), uvec3(r - 1u));
                at = corner.x + corner.y * r + corner.z * r * r;
                candidate = fhedt_label(at) & 255u;
                color = ivec3(int(uint(lookup_palette[candidate * 3u])),
                    int(uint(lookup_palette[candidate * 3u + 1u])),
                    int(uint(lookup_palette[candidate * 3u + 2u])));
                color = (color >> int(lookup_constants[0])) <<
                    int(lookup_constants[0]);
                d = ivec3(query) - color;
                dist2 = uint(d.x * d.x + d.y * d.y + d.z * d.z);
                if (dist2 < minimum) {
                    minimum = dist2;
                    best = candidate;
                }
            }
        }
    }
    return best;
}

/* Fractional FHEDT: a zero-cost grid point certifies only the query that
 * is that seed point. Queries are clamped to the byte range before
 * quantization; uint conversion of a negative float is undefined. */
uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uvec3 cell, corner;
    ivec3 color;
    vec3 gap;
    uint r, at, word, best, candidate, x, y, z;
    float distance, minimum;

    r = fhedt_r();
    cell = uvec3(clamp(query, 0.0, 255.0)) >> lookup_constants[0];
    at = cell.x + cell.y * r + cell.z * r * r;
    word = fhedt_label(at);
    best = word & 255u;
    if (lookup_constants[1] == 0u || (word & 256u) == 0u ||
            (lookup_constants[2] != 0u && (word & 512u) != 0u &&
             all(equal(query, vec3(cell << lookup_constants[0]))))) {
        return best;
    }
    minimum = uintBitsToFloat(0x7f800000u);
    for (z = 0u; z < 2u; ++z) {
        for (y = 0u; y < 2u; ++y) {
            for (x = 0u; x < 2u; ++x) {
                corner = min(cell + uvec3(x, y, z), uvec3(r - 1u));
                at = corner.x + corner.y * r + corner.z * r * r;
                candidate = fhedt_label(at) & 255u;
                color = ivec3(int(uint(lookup_palette[3u * candidate])),
                    int(uint(lookup_palette[3u * candidate + 1u])),
                    int(uint(lookup_palette[3u * candidate + 2u])));
                color = (color >> int(lookup_constants[0])) <<
                    int(lookup_constants[0]);
                gap = query - vec3(color);
                distance = dot(gap, gap);
                if (distance < minimum) {
                    minimum = distance;
                    best = candidate;
                }
            }
        }
    }
    return best;
}
