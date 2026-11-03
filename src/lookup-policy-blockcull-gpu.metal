static uint blockcull_gid(constant Params& params, uint block, uint pos)
{
    uint blocks_x;
    uint x;
    uint y;

    blocks_x = (params.width + 7u) / 8u;
    x = (block % blocks_x) * 8u + pos % 8u;
    y = (block / blocks_x) * 8u + pos / 8u;
    if (x < params.width && y < params.height &&
            y * params.width + x < params.pixel_count) {
        return y * params.width + x;
    }
    return params.pixel_count;
}
kernel void sixel_gpu_palette_apply_blockcull(
    device uchar *result [[buffer(0)]],
    device const uchar *pixels [[buffer(1)]],
    constant uchar *palette [[buffer(2)]],
    device const uchar *transparent_mask [[buffer(3)]],
    device const uchar *blue_noise [[buffer(4)]],
    device const uchar *gradient [[buffer(5)]],
    constant Params& params [[buffer(6)]],
    device const uchar *accumulation [[buffer(7)]],
    device const uchar *accumulation_valid_mask [[buffer(8)]],
    device uchar *accumulation_result_mask [[buffer(9)]],
    device const float *tiles [[buffer(13)]],
    uint group [[threadgroup_position_in_grid]],
    ushort group_simdgroups [[simdgroups_per_threadgroup]],
    ushort lane [[thread_index_in_simdgroup]],
    ushort sg [[simdgroup_index_in_threadgroup]])
{
    threadgroup float rows[8][64 * 4];
    threadgroup float culled[8][(SIXEL_BLOCKCULL_CAP / 8u + 1u) * 32u];
    threadgroup uchar map[8][SIXEL_BLOCKCULL_CAP];
    threadgroup uchar picks[8][64];
    uchar3 query[2];
    uchar3 source[2];
    bool done[2];
    uint gid[2];
    bool spill;
    uint block;
    uint3 low;
    uint3 high;
    int3 color;
    int3 gap;
    int bound;
    uint count;
    uint keep;
    uint slot;
    uint c;
    uint h;
    uint tile_count;
    threadgroup float *column;
    ushort qid;
    ushort fm;
    ushort fn;
    uint row;
    uint first;
    float2 value;
    float2 score;
    float best_score[8];
    uint best_index[8];
    float other_score;
    uint other_index;
    simdgroup_float8x8 a[8];
    simdgroup_float8x8 b;
    simdgroup_float8x8 product;

    block = group * uint(group_simdgroups) + uint(sg);
    low = uint3(255u);
    high = uint3(0u);
    for (h = 0u; h < 2u; ++h) {
        gid[h] = blockcull_gid(params, block, uint(lane) + h * 32u);
        done[h] = prepare_query(result, pixels, transparent_mask,
                                blue_noise, gradient, params, accumulation,
                                accumulation_valid_mask,
                                accumulation_result_mask, gid[h],
                                query[h], source[h]);
        if (!done[h]) {
            low = min(low, uint3(query[h]));
            high = max(high, uint3(query[h]));
        }
        rows[sg][(lane + h * 32u) * 4u + 0u] = float(query[h].x);
        rows[sg][(lane + h * 32u) * 4u + 1u] = float(query[h].y);
        rows[sg][(lane + h * 32u) * 4u + 2u] = float(query[h].z);
        rows[sg][(lane + h * 32u) * 4u + 3u] = 1.0f;
    }
    if (simd_all(done[0] && done[1])) {
        return;
    }
    low = uint3(simd_min(low.x), simd_min(low.y), simd_min(low.z));
    high = uint3(simd_max(high.x), simd_max(high.y), simd_max(high.z));

    bound = 0x7fffffff;
    for (c = lane; c < params.ncolors; c += 32u) {
        color = int3(palette[c * 3u + 0u], palette[c * 3u + 1u],
                     palette[c * 3u + 2u]);
        gap = max(color - int3(low), int3(high) - color);
        bound = min(bound, gap.x * gap.x + gap.y * gap.y + gap.z * gap.z);
    }
    bound = simd_min(bound);

    count = 0u;
    for (uint base = 0u; base < params.ncolors &&
            count <= SIXEL_BLOCKCULL_CAP; base += 32u) {
        c = base + lane;
        keep = 0u;
        color = int3(0);
        if (c < params.ncolors) {
            color = int3(palette[c * 3u + 0u], palette[c * 3u + 1u],
                         palette[c * 3u + 2u]);
            gap = max(max(int3(low) - color, color - int3(high)), int3(0));
            keep = gap.x * gap.x + gap.y * gap.y + gap.z * gap.z <= bound
                ? 1u : 0u;
        }
        slot = count + simd_prefix_exclusive_sum(keep);
        if (keep != 0u && slot < SIXEL_BLOCKCULL_CAP) {
            column = &culled[sg][(slot / 8u) * 32u + slot % 8u];
            column[0] = -2.0f * float(color.x);
            column[8] = -2.0f * float(color.y);
            column[16] = -2.0f * float(color.z);
            column[24] = float(color.x * color.x + color.y * color.y +
                               color.z * color.z);
            map[sg][slot] = uchar(c);
        }
        count += simd_sum(keep);
    }
    spill = count > SIXEL_BLOCKCULL_CAP;
    if (spill) {
        tile_count = (params.ncolors + 7u) / 8u;
    } else {
        /* Unused columns never win; the extra tile only feeds rows 4..7
         * of the last load and must stay finite. */
        tile_count = (count + 7u) / 8u;
        for (slot = count + lane; slot < tile_count * 8u + 8u;
                slot += 32u) {
            column = &culled[sg][(slot / 8u) * 32u + slot % 8u];
            column[0] = 0.0f;
            column[8] = 0.0f;
            column[16] = 0.0f;
            column[24] = slot < tile_count * 8u ? 1.0e30f : 0.0f;
        }
    }
    simdgroup_barrier(mem_flags::mem_threadgroup);

    qid = lane / 4u;
    fm = (qid & 4u) + (lane / 2u) % 4u;
    fn = (qid & 2u) * 2u + (lane % 2u) * 2u;
    for (uint m = 0u; m < 8u; ++m) {
        row = (m * 8u + fm) * 4u;
        value = fn < 4u
            ? float2(rows[sg][row + fn], rows[sg][row + fn + 1u])
            : float2(0.0f);
        a[m].thread_elements()[0] = value.x;
        a[m].thread_elements()[1] = value.y;
        best_score[m] = INFINITY;
        best_index[m] = 0u;
    }
    for (uint t = 0u; t < tile_count; ++t) {
        if (spill) {
            simdgroup_load(b, tiles + t * 64u, 8);
        } else {
            simdgroup_load(b, &culled[sg][t * 32u], 8);
        }
        first = t * 8u + fn;
        for (uint m = 0u; m < 8u; ++m) {
            simdgroup_multiply(product, a[m], b);
            score = float2(product.thread_elements()[0],
                           product.thread_elements()[1]);
            if (score.x < best_score[m]) {
                best_score[m] = score.x;
                best_index[m] = first;
            }
            if (score.y < best_score[m]) {
                best_score[m] = score.y;
                best_index[m] = first + 1u;
            }
        }
    }
    for (uint m = 0u; m < 8u; ++m) {
        for (ushort mask = 1u; mask <= 8u; mask = ushort(mask * 8u)) {
            other_score = simd_shuffle_xor(best_score[m], mask);
            other_index = simd_shuffle_xor(best_index[m], mask);
            if (other_score < best_score[m] ||
                    (other_score == best_score[m] &&
                     other_index < best_index[m])) {
                best_score[m] = other_score;
                best_index[m] = other_index;
            }
        }
        if ((lane & 9u) == 0u) {
            picks[sg][m * 8u + fm] = spill ? uchar(best_index[m])
                                           : map[sg][best_index[m]];
        }
    }
    simdgroup_barrier(mem_flags::mem_threadgroup);
    for (h = 0u; h < 2u; ++h) {
        if (!done[h]) {
            finish_pixel(result, params, accumulation,
                         accumulation_valid_mask, accumulation_result_mask,
                         gid[h], source[h],
                         uint(picks[sg][lane + h * 32u]));
        }
    }
}
