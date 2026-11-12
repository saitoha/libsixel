/* SPDX-License-Identifier: MIT */
/* The blockcull batch ABI: one 32-thread group owns an 8x8 tile,
 * two queries per lane. Consumers supply corrected queries with w=1;
 * inactive queries have w=0. No source, residual or history is visible.
 * This is the collective culling algorithm from the Metal fractional
 * program, also exact for byte queries. Shared reductions deliberately
 * avoid assumptions about the device's hardware subgroup width. */
#define SIXEL_LOOKUP_BATCH 1

shared uvec3 blockcull_lows[32], blockcull_highs[32];
shared uint blockcull_bounds[32], blockcull_prefix[33];
shared uint blockcull_candidates[64];

void lookup_rgb8_batch(vec4 queries[2], uint count, out uvec2 indexes)
{
    uvec3 low, high;
    ivec3 color, gap;
    vec3 difference;
    uint lane, h, i, base, slot, keep, kept, bound, near, far, present;
    uint index, best;
    float minimum, distance;

    lane = gl_LocalInvocationID.x;
    low = uvec3(255u);
    high = uvec3(0u);
    indexes = uvec2(count);
    for (h = 0u; h < 2u; ++h) {
        if (queries[h].w != 0.0) {
            low = min(low, uvec3(floor(queries[h].xyz)));
            high = max(high, uvec3(ceil(queries[h].xyz)));
        }
    }
    blockcull_lows[lane] = low;
    blockcull_highs[lane] = high;
    barrier();
    if (lane == 0u) {
        for (i = 1u; i < 32u; ++i) {
            blockcull_lows[0] = min(blockcull_lows[0], blockcull_lows[i]);
            blockcull_highs[0] = max(blockcull_highs[0], blockcull_highs[i]);
        }
    }
    barrier();
    low = blockcull_lows[0];
    high = blockcull_highs[0];
    if (any(greaterThan(low, high))) return;
    bound = 0xffffffffu;
    for (i = lane; i < count; i += 32u) {
        color = ivec3(lookup_palette[3u*i], lookup_palette[3u*i+1u],
            lookup_palette[3u*i+2u]);
        gap = max(color - ivec3(low), ivec3(high) - color);
        far = uint(gap.x*gap.x + gap.y*gap.y + gap.z*gap.z);
        bound = min(bound, far);
    }
    blockcull_bounds[lane] = bound;
    barrier();
    if (lane == 0u) {
        for (i = 1u; i < 32u; ++i)
            blockcull_bounds[0] = min(blockcull_bounds[0], blockcull_bounds[i]);
    }
    barrier();
    bound = blockcull_bounds[0];
    kept = 0u;
    for (base = 0u; base < count && kept <= 64u; base += 32u) {
        i = base + lane;
        keep = 0u;
        if (i < count) {
            color = ivec3(lookup_palette[3u*i], lookup_palette[3u*i+1u],
                lookup_palette[3u*i+2u]);
            gap = max(max(ivec3(low) - color, color - ivec3(high)), ivec3(0));
            near = uint(gap.x*gap.x + gap.y*gap.y + gap.z*gap.z);
            keep = near <= bound ? 1u : 0u;
        }
        blockcull_prefix[lane] = keep;
        barrier();
        if (lane == 0u) {
            blockcull_prefix[32] = 0u;
            for (i = 0u; i < 32u; ++i) {
                present = blockcull_prefix[i];
                blockcull_prefix[i] = blockcull_prefix[32];
                blockcull_prefix[32] += present;
            }
        }
        barrier();
        slot = kept + blockcull_prefix[lane];
        kept += blockcull_prefix[32];
        /* Protect prefix reads from the next palette chunk's writes. */
        barrier();
        if (keep != 0u && slot < 64u)
            blockcull_candidates[slot] = base + lane;
    }
    barrier();
    for (h = 0u; h < 2u; ++h) {
        if (queries[h].w == 0.0) continue;
        best = 0u;
        minimum = uintBitsToFloat(0x7f800000u);
        /* Overflow scans every entry, never a truncated candidate list.
         * Ascending palette order preserves the first minimum on ties. */
        for (i = 0u; i < (kept > 64u ? count : kept); ++i) {
            index = kept > 64u ? i : blockcull_candidates[i];
            difference = queries[h].xyz - vec3(lookup_palette[3u*index],
                lookup_palette[3u*index+1u], lookup_palette[3u*index+2u]);
            distance = dot(difference, difference);
            if (distance < minimum) { minimum = distance; best = index; }
        }
        indexes[h] = best;
    }
}
