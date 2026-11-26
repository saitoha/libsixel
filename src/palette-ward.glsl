/* SPDX-License-Identifier: MIT */
/* Accel: PALETTE-WARD vulkan
 * vulkan-stages: ward_rows ward_merge
 */
#version 450
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require

layout(local_size_x_id = 0) in;
layout(std430, binding = 0) readonly buffer Centers { vec4 centers[]; };
struct Row { float cost; uint peer; };
layout(std430, binding = 1) buffer Minima { Row rows[]; };
layout(std430, binding = 2) readonly buffer Parameters { uint count, merges; };
layout(std430, binding = 3) writeonly buffer Pairs { uint pairs[]; };

const uint MISSING = 0xffffffffu;
const uint CAPACITY = 768u;

float pair_cost(vec4 a, vec4 b)
{
    vec3 d = a.xyz - b.xyz;
    precise float distance = d.x * d.x + d.y * d.y + d.z * d.z;
    precise float cost = distance * (a.w * b.w / (a.w + b.w));
    return cost;
}

Row initial_best()
{
    return Row(uintBitsToFloat(0x7f800000u), MISSING);
}

Row choose(Row a, Row b)
{
    return b.cost < a.cost || (a.cost == b.cost && b.peer < a.peer) ? b : a;
}

Row reduce_simd(Row value)
{
    float cost = subgroupMin(value.cost);
    return Row(cost, subgroupMin(value.cost == cost ? value.peer : MISSING));
}

void ward_rows()
{
    if (gl_SubgroupID != 0u)
        return;
    uint row = gl_WorkGroupID.x;
    uint lane = subgroupExclusiveAdd(1u), width = subgroupAdd(1u);
    vec4 center = centers[row];
    Row best = initial_best();
    if (center.w > 0.0) {
        for (uint candidate = lane; candidate < count;
             candidate += width) {
            vec4 other = centers[candidate];
            if (candidate != row && other.w > 0.0)
                best = choose(best, Row(pair_cost(center, other), candidate));
        }
    }
    best = reduce_simd(best);
    if (lane == 0u)
        rows[row] = best;
}

shared vec4 current[CAPACITY];
shared Row best_groups[128], repaired[CAPACITY];
shared uint peers[128], dirty_rows[CAPACITY], selected_pairs[CAPACITY];
shared uint kept, removed, dirty_count;

void ward_merge()
{
    if (gl_NumSubgroups > 128u)
        return;
    uint row = gl_LocalInvocationIndex, lane = subgroupExclusiveAdd(1u);
    uint group = gl_SubgroupID, width = subgroupAdd(1u);
    vec4 center = row < count ? centers[row] : vec4(0);
    Row best = row < count ? rows[row] : initial_best();
    if (row < count)
        current[row] = center;
    for (uint step = 0u; step < merges; ++step) {
        Row group_best = reduce_simd(Row(best.cost,
            best.peer == MISSING ? MISSING : row));
        uint neighbor = subgroupMin(row == group_best.peer ? best.peer : MISSING);
        if (lane == 0u) {
            best_groups[group] = group_best;
            peers[group] = neighbor;
        }
        barrier();
        if (group == 0u) {
            Row choice = initial_best();
            for (uint j = lane; j < gl_NumSubgroups; j += width)
                choice = choose(choice, Row(best_groups[j].cost, j));
            Row winner = reduce_simd(choice);
            if (lane == 0u) {
                uint first = best_groups[winner.peer].peer;
                uint second = peers[winner.peer];
                kept = min(first, second);
                removed = max(first, second);
                vec4 a = current[kept], b = current[removed];
                float weight = a.w + b.w;
                precise vec3 mean = (a.xyz * a.w + b.xyz * b.w) / weight;
                current[kept] = vec4(mean, weight);
                current[removed] = vec4(0);
                selected_pairs[step] = (kept << 16u) | removed;
                dirty_count = 0u;
            }
        }
        barrier();
        if (step + 1u == merges)
            break;

        uint dirty = MISSING;
        if (center.w > 0.0) {
            if (row == removed) {
                center = vec4(0);
                best = initial_best();
            } else if (row == kept || best.peer == kept ||
                       best.peer == removed || best.peer == MISSING) {
                center = current[row];
                dirty = atomicAdd(dirty_count, 1u);
                dirty_rows[dirty] = row;
            } else {
                float cost = pair_cost(center, current[kept]);
                // On equal cost the CPU cache keeps the existing partner.
                if (cost < best.cost)
                    best = Row(cost, kept);
            }
        }
        barrier();
        for (uint j = group; j < dirty_count; j += gl_NumSubgroups) {
            uint invalidated = dirty_rows[j];
            vec4 start = current[invalidated];
            Row fresh = initial_best();
            for (uint k = lane; k < count; k += width) {
                vec4 other = current[k];
                if (k != invalidated && other.w > 0.0)
                    fresh = choose(fresh, Row(pair_cost(start, other), k));
            }
            fresh = reduce_simd(fresh);
            if (lane == 0u)
                repaired[j] = fresh;
        }
        barrier();
        if (dirty != MISSING)
            best = repaired[dirty];
    }
    // Keep writes off the dependent merge loop.
    if (row < merges)
        pairs[row] = selected_pairs[row];
}
