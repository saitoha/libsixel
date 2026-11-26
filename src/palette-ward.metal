// SPDX-License-Identifier: MIT
#include <metal_stdlib>
using namespace metal;

#define WARD_CAPACITY 768

struct WardParameters {
    uint count;
    uint merges;
};

struct RowMinimum {
    float cost;
    uint peer;
};

static float
pair_cost(float4 a, float4 b)
{
    const float3 delta = a.xyz - b.xyz;
    const float distance =
        delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    return distance * (a.w * b.w / (a.w + b.w));
}

static RowMinimum
choose(RowMinimum a, RowMinimum b)
{
    return b.cost < a.cost || (b.cost == a.cost && b.peer < a.peer) ? b : a;
}

static RowMinimum
reduce_simd(RowMinimum value)
{
    const float cost = simd_min(value.cost);
    const uint peer = simd_min(value.cost == cost ? value.peer : UINT_MAX);
    return {cost, peer};
}

// Initialize the row-minimum cache in independent SIMD groups.
/* Accel: PALETTE-WARD metal */
kernel void
ward_rows(device const float4 *centers [[buffer(0)]],
          device RowMinimum *rows [[buffer(1)]],
          constant WardParameters &params [[buffer(2)]],
          uint row [[threadgroup_position_in_grid]],
          uint lane [[thread_index_in_simdgroup]],
          uint width [[threads_per_simdgroup]])
{
    const float4 center = centers[row];
    RowMinimum best = {INFINITY, UINT_MAX};
    if (center.w > 0.0f) {
        for (uint peer = lane; peer < params.count; peer += width) {
            const float4 candidate = centers[peer];
            if (peer != row && candidate.w > 0.0f)
                best = choose(best, {pair_cost(center, candidate), peer});
        }
    }
    best = reduce_simd(best);
    if (lane == 0)
        rows[row] = best;
}

// Record the ordered merge pairs. The caller applies them to the original
// double-precision centroids and optional representative statistics.
/* Accel: PALETTE-WARD metal */
kernel void
ward_merge(device const float4 *centers [[buffer(0)]],
           device const RowMinimum *rows [[buffer(1)]],
           constant WardParameters &params [[buffer(2)]],
           device ushort2 *pairs [[buffer(3)]],
           uint row [[thread_index_in_threadgroup]],
           uint lane [[thread_index_in_simdgroup]],
           uint group [[simdgroup_index_in_threadgroup]],
           uint groups [[simdgroups_per_threadgroup]])
{
    threadgroup float4 current[WARD_CAPACITY];
    threadgroup RowMinimum minima[32];
    threadgroup uint peers[32];
    threadgroup uint kept;
    threadgroup uint removed;
    threadgroup atomic_uint dirty_count;
    threadgroup ushort dirty_rows[WARD_CAPACITY];
    threadgroup ushort2 selected_pairs[WARD_CAPACITY];
    threadgroup RowMinimum repaired[WARD_CAPACITY];
    float4 center = row < params.count ? centers[row] : float4(0.0f);
    RowMinimum best = row < params.count ? rows[row] :
        RowMinimum{INFINITY, UINT_MAX};

    if (row < params.count)
        current[row] = center;
    for (uint step = 0; step < params.merges; ++step) {
        // Global ties use the smallest row index, then that row's cached peer.
        const RowMinimum group_best =
            reduce_simd({best.cost, best.peer == UINT_MAX ? UINT_MAX : row});
        const uint peer = simd_shuffle(best.peer, group_best.peer % 32u);
        if (lane == 0) {
            minima[group] = group_best;
            peers[group] = peer;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (group == 0) {
            const RowMinimum group_candidate = lane < groups ?
                RowMinimum{minima[lane].cost, lane} :
                RowMinimum{INFINITY, UINT_MAX};
            const RowMinimum winner = reduce_simd(group_candidate);
            if (lane == 0) {
                const uint selected = minima[winner.peer].peer;
                const uint partner = peers[winner.peer];
                kept = min(selected, partner);
                removed = max(selected, partner);
                const float4 a = current[kept];
                const float4 b = current[removed];
                const float weight = a.w + b.w;
                current[kept] = float4((a.xyz * a.w + b.xyz * b.w) / weight,
                                       weight);
                current[removed] = float4(0.0f);
                selected_pairs[step] = ushort2(kept, removed);
                atomic_store_explicit(&dirty_count, 0u, memory_order_relaxed);
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (step + 1 == params.merges)
            break;

        uint dirty_index = UINT_MAX;
        if (center.w > 0.0f) {
            if (row == removed) {
                center = float4(0.0f);
                best = {INFINITY, UINT_MAX};
            } else if (row == kept || best.peer == kept ||
                       best.peer == removed || best.peer == UINT_MAX) {
                center = current[row];
                dirty_index = atomic_fetch_add_explicit(
                    &dirty_count, 1u, memory_order_relaxed);
                dirty_rows[dirty_index] = (ushort)row;
            } else {
                const float cost = pair_cost(center, current[kept]);
                // The CPU cache retains its incumbent if this cost ties it.
                if (cost < best.cost)
                    best = {cost, kept};
            }
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        const uint count =
            atomic_load_explicit(&dirty_count, memory_order_relaxed);
        // A full SIMD group cooperates on each invalidated row.
        for (uint index = group; index < count; index += groups) {
            const uint target = dirty_rows[index];
            const float4 target_center = current[target];
            RowMinimum fresh = {INFINITY, UINT_MAX};
            for (uint candidate = lane; candidate < params.count;
                 candidate += 32u) {
                const float4 other = current[candidate];
                if (other.w > 0.0f && candidate != target)
                    fresh = choose(fresh,
                        {pair_cost(target_center, other), candidate});
            }
            fresh = reduce_simd(fresh);
            if (lane == 0)
                repaired[index] = fresh;
        }
        threadgroup_barrier(mem_flags::mem_threadgroup);
        if (dirty_index != UINT_MAX)
            best = repaired[dirty_index];
    }
    // Keep writes off the dependent merge loop and publish every pair
    // together after the last shared-memory barrier.
    if (row < params.merges)
        pairs[row] = selected_pairs[row];
}
