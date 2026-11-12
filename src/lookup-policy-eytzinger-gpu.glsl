/* SPDX-License-Identifier: MIT */
/*
 * Eytzinger projection lookup in the "vulkan.lookup-rgb8.v1" program ABI.
 * The builder ports eytzinger_sort of
 * lookup-policy-eytzinger-fractional-gpu.metal: a bounded parallel rank
 * sort orders the palette by the projection key r + g + b, and equal keys
 * keep palette order so scheduling cannot change the ordering.
 *
 * Both entries search the sorted keys for the first key not below the
 * query's, then widen at most six entries to each side, stopping a side
 * once its squared key gap exceeds the best distance. The byte entry keeps
 * eytzinger_palette_lookup() of lookup-policy-eytzinger-gpu.metal, where
 * the first entry found keeps a distance tie; the fractional entry keeps
 * lookup_rgb8_fractional() of the fractional Metal source, where the lower
 * palette index wins a tie. Neither is an exhaustive nearest-color scan.
 *
 * There are no constants. The workspace holds one uint palette index per
 * sorted rank.
 *
 * vulkan-builders: eytzinger_sort
 */
layout(std430, set = 0, binding = SIXEL_LOOKUP_WORKSPACE_BINDING)
    buffer EytzingerWorkspace {
    uint eytzinger_sorted[];
};

const uint EYTZINGER_WINDOW = 6u;

uint eytzinger_key(uint i)
{
    return uint(lookup_palette[3u * i]) + uint(lookup_palette[3u * i + 1u])
        + uint(lookup_palette[3u * i + 2u]);
}

#if defined(SIXEL_LOOKUP_BUILDER)
/* One invocation places one palette entry. */
void eytzinger_sort()
{
    uint i, j, rank, key, other;

    i = gl_GlobalInvocationID.y * gl_NumWorkGroups.x * gl_WorkGroupSize.x
        + gl_GlobalInvocationID.x;
    if (i >= lookup_count) {
        return;
    }
    rank = 0u;
    key = eytzinger_key(i);
    for (j = 0u; j < lookup_count; ++j) {
        other = eytzinger_key(j);
        if (other < key || (other == key && j < i)) {
            ++rank;
        }
    }
    eytzinger_sorted[rank] = i;
}
#endif

/* The first sorted rank whose key is not below key, else the last rank. */
uint eytzinger_rank(float key, uint ncolors)
{
    uint low, high, middle;

    low = 0u;
    high = ncolors;
    while (low < high) {
        middle = low + (high - low) / 2u;
        if (float(eytzinger_key(eytzinger_sorted[middle])) < key) {
            low = middle + 1u;
        } else {
            high = middle;
        }
    }
    return min(low, ncolors - 1u);
}

uint eytzinger_distance(uvec3 query, uint i)
{
    ivec3 gap;

    gap = ivec3(query) - ivec3(int(uint(lookup_palette[3u * i])),
        int(uint(lookup_palette[3u * i + 1u])),
        int(uint(lookup_palette[3u * i + 2u])));
    return uint(gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
}

float eytzinger_fractional_distance(vec3 query, uint i)
{
    vec3 gap;

    gap = query - vec3(float(uint(lookup_palette[3u * i])),
        float(uint(lookup_palette[3u * i + 1u])),
        float(uint(lookup_palette[3u * i + 2u])));
    return dot(gap, gap);
}

uint lookup_rgb8(uvec3 query, uint ncolors)
{
    uint rank, best, best_distance, left, right, step, candidate, index;
    uint distance;
    float key, difference;
    bool left_open, right_open;

    key = float(query.x + query.y + query.z);
    rank = eytzinger_rank(key, ncolors);
    best = eytzinger_sorted[rank];
    best_distance = eytzinger_distance(query, best);
    left = rank;
    right = rank;
    left_open = true;
    right_open = true;
    for (step = 0u; step < EYTZINGER_WINDOW; ++step) {
        if (left_open) {
            if (left == 0u) {
                left_open = false;
            } else {
                candidate = left - 1u;
                index = eytzinger_sorted[candidate];
                difference = key - float(eytzinger_key(index));
                if (difference * difference <= float(best_distance)) {
                    distance = eytzinger_distance(query, index);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best = index;
                    }
                    left = candidate;
                } else {
                    left_open = false;
                }
            }
        }
        if (right_open) {
            if (right + 1u >= ncolors) {
                right_open = false;
            } else {
                candidate = right + 1u;
                index = eytzinger_sorted[candidate];
                difference = key - float(eytzinger_key(index));
                if (difference * difference <= float(best_distance)) {
                    distance = eytzinger_distance(query, index);
                    if (distance < best_distance) {
                        best_distance = distance;
                        best = index;
                    }
                    right = candidate;
                } else {
                    right_open = false;
                }
            }
        }
    }
    return best;
}

uint lookup_rgb8_fractional(vec3 query, uint ncolors)
{
    uint rank, best, left, right, step, candidate, index;
    float key, minimum, distance, difference;
    bool left_open, right_open;

    key = query.x + query.y + query.z;
    rank = eytzinger_rank(key, ncolors);
    best = eytzinger_sorted[rank];
    minimum = eytzinger_fractional_distance(query, best);
    left = rank;
    right = rank;
    left_open = true;
    right_open = true;
    for (step = 0u; step < EYTZINGER_WINDOW; ++step) {
        if (left_open) {
            if (left == 0u) {
                left_open = false;
            } else {
                candidate = left - 1u;
                index = eytzinger_sorted[candidate];
                difference = key - float(eytzinger_key(index));
                if (difference * difference <= minimum) {
                    distance = eytzinger_fractional_distance(query, index);
                    if (distance < minimum ||
                            (distance == minimum && index < best)) {
                        minimum = distance;
                        best = index;
                    }
                    left = candidate;
                } else {
                    left_open = false;
                }
            }
        }
        if (right_open) {
            if (right + 1u >= ncolors) {
                right_open = false;
            } else {
                candidate = right + 1u;
                index = eytzinger_sorted[candidate];
                difference = key - float(eytzinger_key(index));
                if (difference * difference <= minimum) {
                    distance = eytzinger_fractional_distance(query, index);
                    if (distance < minimum ||
                            (distance == minimum && index < best)) {
                        minimum = distance;
                        best = index;
                    }
                    right = candidate;
                } else {
                    right_open = false;
                }
            }
        }
    }
    return best;
}
