/* SPDX-License-Identifier: MIT */
/* Median cut in one threadgroup. A box's best cut depends on its entries
 * alone, so the cuts of all open boxes are computed together: large boxes by
 * the whole threadgroup, small ones by one SIMD group each. SIMD group 0 then
 * replays the CPU's greedy order - split the box of largest SSE, the smaller
 * start on a tie - for as long as the chosen box's cut is known. Values are
 * FP32, so a cut may differ from the double CPU solve where two candidates
 * tie within rounding. */
#include <metal_stdlib>
using namespace metal;

struct HeckbertParams {
    uint count;
    uint colors;
    uint total;
    float weight0;
    float weight1;
    float weight2;
};

struct HeckbertBox {
    uint start;
    uint size;
    uint sum;
    float priority;
};

#define HB_BOXES 256u
#define HB_BINS 256u
#define HB_GROUPS 32u
/* Keep at most six rank-sort entries per lane on each device. */
#define HB_SLOTS 6u
#define HB_ALL (mem_flags::mem_threadgroup | mem_flags::mem_device)

static inline uint
hb_bucket(float value)
{
    return min((uint)max(value, 0.0f), HB_BINS - 1u);
}

static inline float
hb_axis(float4 point, uint axis)
{
    return axis == 0u ? point.x : axis == 1u ? point.y : point.z;
}

static inline float
hb_sse(float3 sums, float3 squares, float population)
{
    float3 component = squares - sums * sums / population;
    float error = 0.0f;
    error += max(component.x, 0.0f);
    error += max(component.y, 0.0f);
    error += max(component.z, 0.0f);
    return error;
}

struct HbMoments {
    float population;
    float3 sums;
    float3 squares;
    uint weight;
};

static inline HbMoments
hb_zero(void)
{
    HbMoments m;
    m.population = 0.0f;
    m.sums = float3(0.0f);
    m.squares = float3(0.0f);
    m.weight = 0u;
    return m;
}

static inline HbMoments
hb_with(HbMoments m, float4 point, float4 origin, float3 weights)
{
    float3 value = (point.xyz - origin.xyz) * weights;
    float3 weighted = point.w * value;
    m.population += point.w;
    m.sums += weighted;
    m.squares += weighted * value;
    m.weight += (uint)point.w;
    return m;
}

static inline HbMoments
hb_prefix(HbMoments value)
{
    HbMoments result;
    result.population = simd_prefix_exclusive_sum(value.population);
    result.sums = simd_prefix_exclusive_sum(value.sums);
    result.squares = simd_prefix_exclusive_sum(value.squares);
    result.weight = simd_prefix_exclusive_sum(value.weight);
    return result;
}

static inline HbMoments
hb_total(HbMoments value)
{
    HbMoments result;
    result.population = simd_sum(value.population);
    result.sums = simd_sum(value.sums);
    result.squares = simd_sum(value.squares);
    result.weight = simd_sum(value.weight);
    return result;
}

static inline HbMoments
hb_add(HbMoments left, HbMoments right)
{
    left.population += right.population;
    left.sums += right.sums;
    left.squares += right.squares;
    left.weight += right.weight;
    return left;
}

static inline uint
hb_widest(float3 low, float3 high, float3 weights)
{
    uint axis = 0u;
    float largest = 0.0f;
    float spread;
    uint g;

    for (g = 0u; g < 3u; ++g) {
        spread = (high[g] - low[g]) * weights[g];
        if (spread > largest) {
            axis = g;
            largest = spread;
        }
    }
    return axis;
}

/* Scan the cuts of one chunk [begin, end) given the moments before it.
 * Keeps the first least total, as the CPU's strict comparison does. */
struct HbCut {
    float best;
    uint cut;
    float left;
    float right;
    uint lower;
};

static inline HbCut
hb_scan(device const float4 *points, device const uint *order, uint start,
        uint size, uint begin, uint end, HbMoments running, HbMoments total,
        float4 origin, float3 weights)
{
    HbCut c;
    uint i;

    c.best = INFINITY;
    c.cut = 0xffffffffu;
    c.left = 0.0f;
    c.right = 0.0f;
    c.lower = 0u;
    for (i = begin; i < end && i + 1u < size; ++i) {
        float left;
        float right;
        running = hb_with(running, points[order[start + i]], origin,
            weights);
        left = hb_sse(running.sums, running.squares, running.population);
        right = hb_sse(total.sums - running.sums,
            total.squares - running.squares,
            total.population - running.population);
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

/* Accel: HECKBERT-MEDIAN-CUT metal */
kernel void
heckbert_solve(device const float4 *points [[buffer(0)]],
               device uint *order [[buffer(1)]],
               device uint *scratch [[buffer(2)]],
               device HeckbertBox *boxes [[buffer(3)]],
#if defined(SIXEL_HECKBERT_RESIDENT)
               device const HeckbertParams &p [[buffer(4)]],
#else
               constant HeckbertParams &p [[buffer(4)]],
#endif
               device uint *result [[buffer(5)]],
               uint tid [[thread_index_in_threadgroup]],
               uint threads [[threads_per_threadgroup]],
               uint lane [[thread_index_in_simdgroup]],
               uint simd_width [[threads_per_simdgroup]],
               uint group [[simdgroup_index_in_threadgroup]],
               uint groups [[simdgroups_per_threadgroup]])
{
    threadgroup uint box_start[HB_BOXES];
    threadgroup uint box_size[HB_BOXES];
    threadgroup uint box_sum[HB_BOXES];
    threadgroup float box_priority[HB_BOXES];
    threadgroup uint cut_at[HB_BOXES];      /* 0: cut not computed yet */
    threadgroup uint cut_lower[HB_BOXES];
    threadgroup float cut_left[HB_BOXES];
    threadgroup float cut_right[HB_BOXES];
    threadgroup atomic_uint hist[HB_BINS];
    threadgroup atomic_uint cursor[HB_BINS];
    threadgroup uint offset[HB_BINS];
    threadgroup uint bin_count[HB_BINS];
    threadgroup float group_float[HB_GROUPS][6];
    threadgroup uint group_uint[HB_GROUPS];
    threadgroup HbMoments group_moments[HB_GROUPS];
    threadgroup uint control[4];  /* boxes, done, axis, winning cut */
    float3 weights = float3(p.weight0, p.weight1, p.weight2);
    uint small_limit = HB_SLOTS * simd_width;
    uint i;

#if defined(SIXEL_HECKBERT_RESIDENT)
    // The representatives stage copies these buckets in first-seen order.
    if (p.count <= p.colors)
        return;
#endif
    for (i = tid; i < p.count; i += threads) {
        order[i] = i;
    }
    if (tid == 0u) {
        box_start[0] = 0u;
        box_size[0] = p.count;
        box_sum[0] = p.total;
        box_priority[0] = 0.0f;
        cut_at[0] = 0u;
        control[0] = 1u;
        control[1] = 0u;
    }
    threadgroup_barrier(HB_ALL);

    for (;;) {
        uint nboxes = control[0];
        uint b;

        /* A. Large open boxes, one at a time across the threadgroup. */
        for (b = 0u; b < nboxes; ++b) {
            uint start;
            uint size;
            uint axis;
            float4 origin;
            if (cut_at[b] != 0u || box_size[b] <= small_limit) {
                continue;
            }
            start = box_start[b];
            size = box_size[b];
            {
                float3 low = float3(INFINITY);
                float3 high = float3(-INFINITY);
                for (i = tid; i < size; i += threads) {
                    float3 value = points[order[start + i]].xyz;
                    low = min(low, value);
                    high = max(high, value);
                }
                low = simd_min(low);
                high = simd_max(high);
                if (lane == 0u) {
                    group_float[group][0] = low.x;
                    group_float[group][1] = low.y;
                    group_float[group][2] = low.z;
                    group_float[group][3] = high.x;
                    group_float[group][4] = high.y;
                    group_float[group][5] = high.z;
                }
            }
            for (i = tid; i < HB_BINS; i += threads) {
                atomic_store_explicit(&hist[i], 0u, memory_order_relaxed);
                atomic_store_explicit(&cursor[i], 0u, memory_order_relaxed);
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (tid == 0u) {
                float3 lo = float3(INFINITY);
                float3 hi = float3(-INFINITY);
                uint g;
                for (g = 0u; g < groups; ++g) {
                    lo = min(lo, float3(group_float[g][0], group_float[g][1],
                        group_float[g][2]));
                    hi = max(hi, float3(group_float[g][3], group_float[g][4],
                        group_float[g][5]));
                }
                control[2] = hb_widest(lo, hi, weights);
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            axis = control[2];
            for (i = tid; i < size; i += threads) {
                atomic_fetch_add_explicit(&hist[hb_bucket(hb_axis(
                    points[order[start + i]], axis))], 1u,
                    memory_order_relaxed);
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (tid < HB_BINS) {
                uint count = atomic_load_explicit(&hist[tid],
                    memory_order_relaxed);
                uint before = simd_prefix_exclusive_sum(count);
                uint total = simd_sum(count);
                bin_count[tid] = count;
                offset[tid] = before;
                if (lane == 0u) {
                    group_uint[group] = total;
                }
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            if (tid < HB_BINS) {
                uint g;
                uint base = 0u;
                for (g = 0u; g < group; ++g) {
                    base += group_uint[g];
                }
                offset[tid] += base;
            }
            threadgroup_barrier(mem_flags::mem_threadgroup);
            for (i = tid; i < size; i += threads) {
                uint entry = order[start + i];
                uint bucket = hb_bucket(hb_axis(points[entry], axis));
                uint slot = offset[bucket] + atomic_fetch_add_explicit(
                    &cursor[bucket], 1u, memory_order_relaxed);
                scratch[start + slot] = entry;
            }
            threadgroup_barrier(HB_ALL);
            for (i = tid; i < size; i += threads) {
                uint entry = scratch[start + i];
                float value = hb_axis(points[entry], axis);
                uint bucket = hb_bucket(value);
                uint low = offset[bucket];
                uint high = low + bin_count[bucket];
                uint rank = low;
                uint q;
                for (q = low; q < high; ++q) {
                    uint other = scratch[start + q];
                    float compared = hb_axis(points[other], axis);
                    rank += (compared < value ||
                        (compared == value && other < entry)) ? 1u : 0u;
                }
                order[start + rank] = entry;
            }
            threadgroup_barrier(HB_ALL);
            origin = points[order[start]];
            {
                uint chunk = (size + threads - 1u) / threads;
                uint begin = min(tid * chunk, size);
                uint end = min(begin + chunk, size);
                HbMoments local = hb_zero();
                HbMoments before;
                HbMoments total;
                HbCut c;
                uint g;

                for (i = begin; i < end; ++i) {
                    local = hb_with(local, points[order[start + i]], origin,
                        weights);
                }
                before = hb_prefix(local);
                total = hb_total(local);
                if (lane == 0u) {
                    group_moments[group] = total;
                }
                threadgroup_barrier(mem_flags::mem_threadgroup);
                for (g = 0u; g < group; ++g) {
                    before = hb_add(before, group_moments[g]);
                }
                total = group_moments[0];
                for (g = 1u; g < groups; ++g) {
                    total = hb_add(total, group_moments[g]);
                }
                c = hb_scan(points, order, start, size, begin, end, before,
                    total, origin, weights);
                {
                    float top = simd_min(c.best);
                    uint first = simd_min(c.best == top ? c.cut
                        : 0xffffffffu);
                    if (lane == 0u) {
                        group_float[group][0] = top;
                        group_uint[group] = first;
                    }
                }
                threadgroup_barrier(mem_flags::mem_threadgroup);
                if (group == 0u) {
                    float top = INFINITY;
                    uint first = 0xffffffffu;
                    float all_top;
                    uint all_first;

                    /* Narrow SIMD groups combine several partial cuts
                     * per lane before the final reduction. */
                    for (g = lane; g < groups; g += simd_width) {
                        float score = group_float[g][0];
                        uint cut = group_uint[g];
                        if (score < top || (score == top && cut < first)) {
                            top = score;
                            first = cut;
                        }
                    }
                    all_top = simd_min(top);
                    all_first = simd_min(top == all_top ? first
                        : 0xffffffffu);
                    if (lane == 0u) {
                        control[3] = all_first;
                    }
                }
                threadgroup_barrier(mem_flags::mem_threadgroup);
                if (c.cut != 0xffffffffu && c.cut == control[3]) {
                    cut_at[b] = c.cut;
                    cut_lower[b] = c.lower;
                    cut_left[b] = c.left;
                    cut_right[b] = c.right;
                }
                threadgroup_barrier(mem_flags::mem_threadgroup);
            }
        }

        /* B. Small open boxes, one SIMD group each. */
        for (b = group; b < nboxes; b += groups) {
            uint start;
            uint size;
            uint axis;
            uint chunk;
            uint begin;
            uint end;
            float4 origin;
            float3 low;
            float3 high;
            HbMoments local;
            HbMoments before;
            HbMoments total;
            HbCut c;
            float top;
            uint first;
            if (cut_at[b] != 0u || box_size[b] > small_limit ||
                    box_size[b] < 2u) {
                continue;
            }
            start = box_start[b];
            size = box_size[b];
            low = float3(INFINITY);
            high = float3(-INFINITY);
            for (i = lane; i < size; i += simd_width) {
                float3 value = points[order[start + i]].xyz;
                low = min(low, value);
                high = max(high, value);
            }
            axis = hb_widest(simd_min(low), simd_max(high), weights);
            /* Rank sort in registers: each lane holds up to HB_SLOTS
             * entries and sees every other entry by shuffle. */
            {
                float values[HB_SLOTS];
                uint ids[HB_SLOTS];
                uint ranks[HB_SLOTS];
                uint j;
                uint l;
                uint m;
                for (j = 0u; j < HB_SLOTS; ++j) {
                    uint q = lane + j * simd_width;
                    ids[j] = q < size ? order[start + q] : 0xffffffffu;
                    values[j] = q < size ? hb_axis(points[ids[j]], axis)
                        : INFINITY;
                    ranks[j] = 0u;
                }
                for (j = 0u; j < HB_SLOTS; ++j) {
                    if (j * simd_width >= size) {
                        break;
                    }
                    for (l = 0u; l < simd_width; ++l) {
                        float value = simd_shuffle(values[j], (ushort)l);
                        uint id = simd_shuffle(ids[j], (ushort)l);
                        if (id == 0xffffffffu) {
                            break;
                        }
                        for (m = 0u; m < HB_SLOTS; ++m) {
                            ranks[m] += (value < values[m] ||
                                (value == values[m] && id < ids[m]))
                                ? 1u : 0u;
                        }
                    }
                }
                for (j = 0u; j < HB_SLOTS; ++j) {
                    if (ids[j] != 0xffffffffu) {
                        scratch[start + ranks[j]] = ids[j];
                    }
                }
            }
            simdgroup_barrier(mem_flags::mem_device);
            for (i = lane; i < size; i += simd_width) {
                order[start + i] = scratch[start + i];
            }
            simdgroup_barrier(mem_flags::mem_device);
            origin = points[order[start]];
            chunk = (size + simd_width - 1u) / simd_width;
            begin = min(lane * chunk, size);
            end = min(begin + chunk, size);
            local = hb_zero();
            for (i = begin; i < end; ++i) {
                local = hb_with(local, points[order[start + i]], origin,
                    weights);
            }
            before = hb_prefix(local);
            total = hb_total(local);
            c = hb_scan(points, order, start, size, begin, end, before,
                total, origin, weights);
            top = simd_min(c.best);
            first = simd_min(c.best == top ? c.cut : 0xffffffffu);
            if (c.cut != 0xffffffffu && c.cut == first) {
                cut_at[b] = c.cut;
                cut_lower[b] = c.lower;
                cut_left[b] = c.left;
                cut_right[b] = c.right;
            }
        }
        threadgroup_barrier(HB_ALL);

        /* C. Greedy replay on SIMD group 0 while the chosen cut is known. */
        if (group == 0u) {
            for (;;) {
                float best = -INFINITY;
                uint best_start = 0xffffffffu;
                uint best_index = 0xffffffffu;
                float top;
                uint first;
                uint chosen;
                nboxes = control[0];
                if (nboxes >= p.colors) {
                    if (lane == 0u) {
                        control[1] = 1u;
                    }
                    break;
                }
                for (i = lane; i < nboxes; i += simd_width) {
                    if (box_size[i] >= 2u &&
                            (box_priority[i] > best ||
                             (box_priority[i] == best &&
                              box_start[i] < best_start))) {
                        best = box_priority[i];
                        best_start = box_start[i];
                        best_index = i;
                    }
                }
                top = simd_max(best);
                first = simd_min(best == top ? best_start : 0xffffffffu);
                chosen = simd_min(best == top && best_start == first
                    ? best_index : 0xffffffffu);
                if (chosen == 0xffffffffu) {
                    if (lane == 0u) {
                        control[1] = 1u;
                    }
                    break;
                }
                if (cut_at[chosen] == 0u) {
                    break;
                }
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
                simdgroup_barrier(mem_flags::mem_threadgroup);
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (control[1] != 0u) {
            break;
        }
    }

    for (i = tid; i < control[0]; i += threads) {
        boxes[i].start = box_start[i];
        boxes[i].size = box_size[i];
        boxes[i].sum = box_sum[i];
        boxes[i].priority = box_priority[i];
    }
    if (tid == 0u) {
        result[0] = control[0];
    }
}
