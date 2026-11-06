#include <metal_stdlib>
using namespace metal;

static uint eytzinger_key(constant uchar *palette, uint i)
{
    return uint(palette[3*i]) + palette[3*i+1] + palette[3*i+2];
}

/* A bounded parallel rank sort constructs the same projection ordering.
 * Equal projections use palette order so GPU scheduling cannot change it. */
kernel void eytzinger_sort(constant uchar *palette [[buffer(0)]],
    constant uint &count [[buffer(1)]],
    device uint *sorted [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    uint rank, key, other, j;

    if (i >= count) return;
    rank = 0;
    key = eytzinger_key(palette, i);
    for (j = 0; j < count; ++j) {
        other = eytzinger_key(palette, j);
        rank += other < key || (other == key && j < i);
    }
    sorted[rank] = i;
}

static float eytzinger_distance(float3 query, constant uchar *palette,
    uint i)
{
    float3 gap;

    gap = query-float3(palette[3*i], palette[3*i+1], palette[3*i+2]);
    return dot(gap, gap);
}

/* Preserve the byte policy's approximate six-neighbor window and heuristic
 * projection stop. Only the query and distance arithmetic become fractional;
 * this is deliberately not advertised as an exhaustive nearest-color scan. */
static uint lookup_rgb8_fractional(float3 query, constant uchar *palette,
    uint count, constant uchar *configuration,
    device const uchar *workspace)
{
    device const uint *sorted;
    uint low, high, middle, rank, best, left, right, step, candidate, index;
    float key, minimum, distance, difference;
    bool left_open, right_open;

    (void)configuration;
    sorted = (device const uint *)workspace;
    key = query.x + query.y + query.z;
    low = 0;
    high = count;
    while (low < high) {
        middle = low+(high-low)/2;
        if (float(eytzinger_key(palette, sorted[middle])) < key)
            low = middle+1;
        else high = middle;
    }
    rank = min(low, count-1);
    best = sorted[rank];
    minimum = eytzinger_distance(query, palette, best);
    left = right = rank;
    left_open = right_open = true;
    for (step = 0; step < 6; ++step) {
        if (left_open) {
            if (left == 0) left_open = false;
            else {
                candidate = left-1;
                index = sorted[candidate];
                difference = key-float(eytzinger_key(palette, index));
                if (difference*difference <= minimum) {
                    distance = eytzinger_distance(query, palette, index);
                    if (distance < minimum ||
                            (distance == minimum && index < best)) {
                        minimum = distance; best = index;
                    }
                    left = candidate;
                } else left_open = false;
            }
        }
        if (right_open) {
            if (right+1 >= count) right_open = false;
            else {
                candidate = right+1;
                index = sorted[candidate];
                difference = key-float(eytzinger_key(palette, index));
                if (difference*difference <= minimum) {
                    distance = eytzinger_distance(query, palette, index);
                    if (distance < minimum ||
                            (distance == minimum && index < best)) {
                        minimum = distance; best = index;
                    }
                    right = candidate;
                } else right_open = false;
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
