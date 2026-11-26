/* SPDX-License-Identifier: MIT */
/* Accel: HECKBERT-MEDIAN-CUT vulkan */
/* A large box uses the whole workgroup. Small boxes use one subgroup each;
 * subgroup zero then replays the greedy split order, as in the Metal solver.
 * vulkan-stages: heckbert_solve
 */
#version 450
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_shuffle : require
#extension GL_KHR_shader_subgroup_arithmetic : require
layout(local_size_x_id = 0) in;
layout(std430, binding = 0) readonly buffer Points { vec4 points[]; };
layout(std430, binding = 1) buffer Order { uint order[]; };
layout(std430, binding = 2) buffer Scratch { uint scratch[]; };
struct HbBox { uint start; uint size; uint sum; float priority; };
layout(std430, binding = 3) buffer Boxes { HbBox boxes[]; };
layout(std430, binding = 4) readonly buffer Parameters {
    uint count;
    uint colors;
    uint total_weight;
    float weight0;
    float weight1;
    float weight2;
};
layout(std430, binding = 5) buffer Result { uint result[]; };

/* Resident Ward oversplits before merging back to the wire palette. */
layout(constant_id = 1) const uint HB_BOXES = 256u;
#define HB_BINS 256u
#define HB_GROUPS 128u
#define HB_SLOTS 6u

struct HbMoments {
    float population;
    vec3 sums;
    vec3 squares;
    uint weight;
};
struct HbCut {
    float best;
    uint cut;
    float left;
    float right;
    uint lower;
};

shared uint box_start[HB_BOXES];
shared uint box_size[HB_BOXES];
shared uint box_sum[HB_BOXES];
shared float box_priority[HB_BOXES];
shared uint cut_at[HB_BOXES];
shared uint cut_lower[HB_BOXES];
shared float cut_left[HB_BOXES];
shared float cut_right[HB_BOXES];
shared uint hist[HB_BINS];
shared uint cursor[HB_BINS];
shared uint offset[HB_BINS];
shared uint bin_count[HB_BINS];
shared float group_float[HB_GROUPS][6];
shared uint group_uint[HB_GROUPS];
shared HbMoments group_moments[HB_GROUPS];
shared uint control[4];

uint hb_bucket(float value)
{
    return min(uint(max(value, 0.0)), HB_BINS - 1u);
}

float hb_axis(vec4 point, uint axis)
{
    return axis == 0u ? point.x : axis == 1u ? point.y : point.z;
}

float hb_sse(vec3 sums, vec3 squares, float population)
{
    vec3 component = squares - sums * sums / population;
    float error = 0.0;
    error += max(component.x, 0.0);
    error += max(component.y, 0.0);
    error += max(component.z, 0.0);
    return error;
}

HbMoments hb_zero()
{
    HbMoments m;
    m.population = 0.0;
    m.sums = vec3(0.0);
    m.squares = vec3(0.0);
    m.weight = 0u;
    return m;
}

HbMoments hb_with(HbMoments m, vec4 point, vec4 origin, vec3 weights)
{
    vec3 value = (point.xyz - origin.xyz) * weights;
    vec3 weighted = point.w * value;
    m.population += point.w;
    m.sums += weighted;
    m.squares += weighted * value;
    m.weight += uint(point.w);
    return m;
}

HbMoments hb_prefix(HbMoments value)
{
    HbMoments result_m;
    result_m.population = subgroupExclusiveAdd(value.population);
    result_m.sums = subgroupExclusiveAdd(value.sums);
    result_m.squares = subgroupExclusiveAdd(value.squares);
    result_m.weight = subgroupExclusiveAdd(value.weight);
    return result_m;
}

HbMoments hb_total(HbMoments value)
{
    HbMoments result_m;
    result_m.population = subgroupAdd(value.population);
    result_m.sums = subgroupAdd(value.sums);
    result_m.squares = subgroupAdd(value.squares);
    result_m.weight = subgroupAdd(value.weight);
    return result_m;
}

HbMoments hb_add(HbMoments left_m, HbMoments right_m)
{
    left_m.population += right_m.population;
    left_m.sums += right_m.sums;
    left_m.squares += right_m.squares;
    left_m.weight += right_m.weight;
    return left_m;
}

uint hb_widest(vec3 low, vec3 high, vec3 weights)
{
    uint axis = 0u;
    float largest = 0.0;
    for (uint g = 0u; g < 3u; ++g) {
        float spread = (high[g] - low[g]) * weights[g];
        if (spread > largest) {
            axis = g;
            largest = spread;
        }
    }
    return axis;
}

HbCut hb_scan(uint start, uint size, uint begin, uint end,
              HbMoments running, HbMoments total_m,
              vec4 origin, vec3 weights)
{
    HbCut c;
    c.best = 1.0 / 0.0;
    c.cut = 0xffffffffu;
    c.left = 0.0;
    c.right = 0.0;
    c.lower = 0u;
    for (uint i = begin; i < end && i + 1u < size; ++i) {
        running = hb_with(running, points[order[start + i]], origin,
            weights);
        float left = hb_sse(running.sums, running.squares,
            running.population);
        float right = hb_sse(total_m.sums - running.sums,
            total_m.squares - running.squares,
            total_m.population - running.population);
        if (left + right < c.best) {
            c.best = left + right;
            c.cut = i + 1u;
            c.left = left;
            c.right = right;
            c.lower = running.weight;
        }
    }
    return c;
}

void heckbert_solve()
{
    uint tid = gl_LocalInvocationID.x;
    uint threads = gl_WorkGroupSize.x;
    uint lane = gl_SubgroupInvocationID;
    /* Intel may execute 16 active lanes while advertising a 32-lane
     * subgroup. Use the participating width for shuffles and box slots. */
    uint width = subgroupAdd(1u);
    uint group = gl_SubgroupID;
    uint groups = gl_NumSubgroups;
    uint small_limit = HB_SLOTS * width;
    vec3 weights = vec3(weight0, weight1, weight2);

    for (uint i = tid; i < count; i += threads) order[i] = i;
    if (tid == 0u) {
        box_start[0] = 0u;
        box_size[0] = count;
        box_sum[0] = total_weight;
        box_priority[0] = 0.0;
        cut_at[0] = 0u;
        control[0] = 1u;
        control[1] = 0u;
    }
    memoryBarrierBuffer();
    barrier();

    for (;;) {
        uint nboxes = control[0];
        /* Large boxes: identical bucket and within-bucket rank order to
         * Metal. Every subgroup helps one box before moving to the next. */
        for (uint b = 0u; b < nboxes; ++b) {
            if (cut_at[b] != 0u || box_size[b] <= small_limit) continue;
            uint start = box_start[b];
            uint size = box_size[b];
            vec3 low = vec3(1.0 / 0.0);
            vec3 high = vec3(-1.0 / 0.0);
            for (uint i = tid; i < size; i += threads) {
                vec3 value = points[order[start + i]].xyz;
                low = min(low, value);
                high = max(high, value);
            }
            low = subgroupMin(low);
            high = subgroupMax(high);
            if (lane == 0u) {
                group_float[group][0] = low.x;
                group_float[group][1] = low.y;
                group_float[group][2] = low.z;
                group_float[group][3] = high.x;
                group_float[group][4] = high.y;
                group_float[group][5] = high.z;
            }
            for (uint i = tid; i < HB_BINS; i += threads) {
                hist[i] = 0u;
                cursor[i] = 0u;
            }
            barrier();
            if (tid == 0u) {
                vec3 lo = vec3(1.0 / 0.0);
                vec3 hi = vec3(-1.0 / 0.0);
                for (uint g = 0u; g < groups; ++g) {
                    lo = min(lo, vec3(group_float[g][0],
                        group_float[g][1], group_float[g][2]));
                    hi = max(hi, vec3(group_float[g][3],
                        group_float[g][4], group_float[g][5]));
                }
                control[2] = hb_widest(lo, hi, weights);
            }
            barrier();
            uint axis = control[2];
            for (uint i = tid; i < size; i += threads) {
                atomicAdd(hist[hb_bucket(hb_axis(
                    points[order[start + i]], axis))], 1u);
            }
            barrier();
            if (tid < HB_BINS) {
                uint n = hist[tid];
                uint subtotal = subgroupAdd(n);
                bin_count[tid] = n;
                offset[tid] = subgroupExclusiveAdd(n);
                if (lane == 0u) group_uint[group] = subtotal;
            }
            barrier();
            if (tid < HB_BINS) {
                uint base = 0u;
                for (uint g = 0u; g < group; ++g) base += group_uint[g];
                offset[tid] += base;
            }
            barrier();
            for (uint i = tid; i < size; i += threads) {
                uint entry = order[start + i];
                uint bucket = hb_bucket(hb_axis(points[entry], axis));
                uint slot = offset[bucket] + atomicAdd(cursor[bucket], 1u);
                scratch[start + slot] = entry;
            }
            memoryBarrierBuffer();
            barrier();
            for (uint i = tid; i < size; i += threads) {
                uint entry = scratch[start + i];
                float value = hb_axis(points[entry], axis);
                uint bucket = hb_bucket(value);
                uint lower = offset[bucket];
                uint upper = lower + bin_count[bucket];
                uint rank = lower;
                for (uint q = lower; q < upper; ++q) {
                    uint other = scratch[start + q];
                    float compared = hb_axis(points[other], axis);
                    rank += uint(compared < value ||
                        (compared == value && other < entry));
                }
                order[start + rank] = entry;
            }
            memoryBarrierBuffer();
            barrier();
            vec4 origin = points[order[start]];
            uint chunk = (size + threads - 1u) / threads;
            uint begin = min(tid * chunk, size);
            uint end = min(begin + chunk, size);
            HbMoments local_m = hb_zero();
            for (uint i = begin; i < end; ++i) {
                local_m = hb_with(local_m, points[order[start + i]],
                    origin, weights);
            }
            HbMoments before = hb_prefix(local_m);
            HbMoments total_m = hb_total(local_m);
            if (lane == 0u) group_moments[group] = total_m;
            barrier();
            for (uint g = 0u; g < group; ++g)
                before = hb_add(before, group_moments[g]);
            total_m = group_moments[0];
            for (uint g = 1u; g < groups; ++g)
                total_m = hb_add(total_m, group_moments[g]);
            HbCut c = hb_scan(start, size, begin, end, before, total_m,
                origin, weights);
            float top = subgroupMin(c.best);
            uint first = subgroupMin(c.best == top ? c.cut : 0xffffffffu);
            if (lane == 0u) {
                group_float[group][0] = top;
                group_uint[group] = first;
            }
            barrier();
            if (group == 0u) {
                top = 1.0 / 0.0;
                first = 0xffffffffu;
                for (uint g = lane; g < groups; g += width) {
                    float score = group_float[g][0];
                    uint cut = group_uint[g];
                    if (score < top || (score == top && cut < first)) {
                        top = score;
                        first = cut;
                    }
                }
                float all_top = subgroupMin(top);
                uint all_first = subgroupMin(top == all_top ? first
                    : 0xffffffffu);
                if (lane == 0u) control[3] = all_first;
            }
            barrier();
            if (c.cut != 0xffffffffu && c.cut == control[3]) {
                cut_at[b] = c.cut;
                cut_lower[b] = c.lower;
                cut_left[b] = c.left;
                cut_right[b] = c.right;
            }
            barrier();
        }

        /* Each subgroup sorts one small box in registers. Rank ties use
         * original entry indices, independent of subgroup scheduling. */
        for (uint b = group; b < nboxes; b += groups) {
            if (cut_at[b] != 0u || box_size[b] > small_limit ||
                    box_size[b] < 2u) continue;
            uint start = box_start[b];
            uint size = box_size[b];
            vec3 low = vec3(1.0 / 0.0);
            vec3 high = vec3(-1.0 / 0.0);
            for (uint i = lane; i < size; i += width) {
                vec3 value = points[order[start + i]].xyz;
                low = min(low, value);
                high = max(high, value);
            }
            uint axis = hb_widest(subgroupMin(low), subgroupMax(high),
                weights);
            float values[HB_SLOTS];
            uint ids[HB_SLOTS];
            uint ranks[HB_SLOTS];
            for (uint j = 0u; j < HB_SLOTS; ++j) {
                uint q = lane + j * width;
                ids[j] = q < size ? order[start + q] : 0xffffffffu;
                values[j] = q < size ? hb_axis(points[ids[j]], axis)
                    : 1.0 / 0.0;
                ranks[j] = 0u;
            }
            for (uint j = 0u; j < HB_SLOTS; ++j) {
                float candidate_value = values[j];
                uint candidate_id = ids[j];
                if (j * width >= size) break;
                for (uint l = 0u; l < width; ++l) {
                    float value = subgroupShuffle(candidate_value, l);
                    uint id = subgroupShuffle(candidate_id, l);
                    if (id == 0xffffffffu) break;
                    for (uint m = 0u; m < HB_SLOTS; ++m) {
                        ranks[m] += uint(value < values[m] ||
                            (value == values[m] && id < ids[m]));
                    }
                }
            }
            for (uint j = 0u; j < HB_SLOTS; ++j) {
                if (ids[j] != 0xffffffffu)
                    scratch[start + ranks[j]] = ids[j];
            }
            subgroupMemoryBarrierBuffer();
            subgroupBarrier();
            for (uint i = lane; i < size; i += width)
                order[start + i] = scratch[start + i];
            subgroupMemoryBarrierBuffer();
            subgroupBarrier();
            vec4 origin = points[order[start]];
            uint chunk = (size + width - 1u) / width;
            uint begin = min(lane * chunk, size);
            uint end = min(begin + chunk, size);
            HbMoments local_m = hb_zero();
            for (uint i = begin; i < end; ++i)
                local_m = hb_with(local_m, points[order[start + i]],
                    origin, weights);
            HbMoments before = hb_prefix(local_m);
            HbMoments total_m = hb_total(local_m);
            HbCut c = hb_scan(start, size, begin, end, before, total_m,
                origin, weights);
            float top = subgroupMin(c.best);
            uint first = subgroupMin(c.best == top ? c.cut : 0xffffffffu);
            if (c.cut != 0xffffffffu && c.cut == first) {
                cut_at[b] = c.cut;
                cut_lower[b] = c.lower;
                cut_left[b] = c.left;
                cut_right[b] = c.right;
            }
        }
        memoryBarrierBuffer();
        barrier();

        /* Replay only the cuts that the parallel phase has already solved.
         * A missing cut starts another large/small pass for the new box. */
        if (group == 0u) {
            for (;;) {
                float best = -1.0 / 0.0;
                uint best_start = 0xffffffffu;
                uint best_index = 0xffffffffu;
                nboxes = control[0];
                if (nboxes >= colors) {
                    if (lane == 0u) control[1] = 1u;
                    break;
                }
                for (uint i = lane; i < nboxes; i += width) {
                    if (box_size[i] >= 2u &&
                            (box_priority[i] > best ||
                             (box_priority[i] == best &&
                              box_start[i] < best_start))) {
                        best = box_priority[i];
                        best_start = box_start[i];
                        best_index = i;
                    }
                }
                float top = subgroupMax(best);
                uint first = subgroupMin(best == top ? best_start
                    : 0xffffffffu);
                uint chosen = subgroupMin(best == top &&
                    best_start == first ? best_index : 0xffffffffu);
                if (chosen == 0xffffffffu) {
                    if (lane == 0u) control[1] = 1u;
                    break;
                }
                if (cut_at[chosen] == 0u) break;
                if (lane == 0u) {
                    uint cut = cut_at[chosen];
                    uint lower = cut_lower[chosen];
                    box_start[nboxes] = box_start[chosen] + cut;
                    box_size[nboxes] = box_size[chosen] - cut;
                    box_sum[nboxes] = box_sum[chosen] - lower;
                    box_priority[nboxes] = cut_right[chosen];
                    cut_at[nboxes] = 0u;
                    box_size[chosen] = cut;
                    box_sum[chosen] = lower;
                    box_priority[chosen] = cut_left[chosen];
                    cut_at[chosen] = 0u;
                    control[0] = nboxes + 1u;
                }
                subgroupBarrier();
            }
        }
        barrier();
        if (control[1] != 0u) break;
    }

    for (uint i = tid; i < control[0]; i += threads) {
        boxes[i].start = box_start[i];
        boxes[i].size = box_size[i];
        boxes[i].sum = box_sum[i];
        boxes[i].priority = box_priority[i];
    }
    if (tid == 0u) result[0] = control[0];
}
