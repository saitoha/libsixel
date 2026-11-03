/*
 * Exhaustive direct lookup as a matrix product.  For byte coordinates,
 *
 *   |q - c|^2 - |q|^2 = |c|^2 - 2 q.c
 *
 * ranks colors like the direct scan, and every term is an integer below
 * 2^24, so float products and sums are exact in any order.  Each
 * simdgroup multiplies its 32 queries (rows r, g, b, 1) by 8x8 palette
 * tiles (columns -2r, -2g, -2b, |c|^2) and keeps the first minimum, so
 * ties return the lowest index as the scan does.  Every lane takes part
 * in the matrix operations; finished pixels supply a zero query and keep
 * the result they already wrote.
 *
 * Lane layout of simdgroup_float8x8::thread_elements(): lane owns row fm,
 * columns fn and fn + 1, with qid = lane / 4,
 * fm = (qid & 4) + (lane / 2) % 4 and fn = (qid & 2) * 2 + (lane % 2) * 2.
 * The four lanes of a row differ in lane bits 0 and 3.
 */
kernel void sixel_gpu_palette_apply_gemm(
    device uchar *result [[buffer(0)]],
    device const uchar *pixels [[buffer(1)]],
    device const uchar *transparent_mask [[buffer(3)]],
    device const uchar *blue_noise [[buffer(4)]],
    device const uchar *gradient [[buffer(5)]],
    constant Params& params [[buffer(6)]],
    device const uchar *accumulation [[buffer(7)]],
    device const uchar *accumulation_valid_mask [[buffer(8)]],
    device uchar *accumulation_result_mask [[buffer(9)]],
    device const float *tiles [[buffer(13)]],
    uint gid [[thread_position_in_grid]],
    ushort lane [[thread_index_in_simdgroup]],
    ushort sg [[simdgroup_index_in_threadgroup]])
{
    threadgroup float rows[8][32 * 4];
    threadgroup uchar picks[8][32];
    uchar3 query;
    uchar3 source;
    bool done;
    ushort qid;
    ushort fm;
    ushort fn;
    uint row;
    uint tile_count;
    uint first;
    float2 value;
    float2 score;
    float best_score[4];
    uint best_index[4];
    float other_score;
    uint other_index;
    simdgroup_float8x8 a[4];
    simdgroup_float8x8 b;
    simdgroup_float8x8 c;

    done = prepare_query(result, pixels, transparent_mask, blue_noise,
                         gradient, params, accumulation,
                         accumulation_valid_mask, accumulation_result_mask,
                         gid, query, source);
    rows[sg][lane * 4u + 0u] = float(query.x);
    rows[sg][lane * 4u + 1u] = float(query.y);
    rows[sg][lane * 4u + 2u] = float(query.z);
    rows[sg][lane * 4u + 3u] = 1.0f;
    simdgroup_barrier(mem_flags::mem_threadgroup);

    qid = lane / 4u;
    fm = (qid & 4u) + (lane / 2u) % 4u;
    fn = (qid & 2u) * 2u + (lane % 2u) * 2u;
    for (uint m = 0u; m < 4u; ++m) {
        row = (m * 8u + fm) * 4u;
        value = fn < 4u
            ? float2(rows[sg][row + fn], rows[sg][row + fn + 1u])
            : float2(0.0f);
        a[m].thread_elements()[0] = value.x;
        a[m].thread_elements()[1] = value.y;
        best_score[m] = INFINITY;
        best_index[m] = 0u;
    }
    tile_count = (params.ncolors + 7u) / 8u;
    for (uint t = 0u; t < tile_count; ++t) {
        simdgroup_load(b, tiles + t * 64u, 8);
        first = t * 8u + fn;
        for (uint m = 0u; m < 4u; ++m) {
            simdgroup_multiply(c, a[m], b);
            score = float2(c.thread_elements()[0], c.thread_elements()[1]);
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
    for (uint m = 0u; m < 4u; ++m) {
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
            picks[sg][m * 8u + fm] = uchar(best_index[m]);
        }
    }
    simdgroup_barrier(mem_flags::mem_threadgroup);
    if (!done) {
        finish_pixel(result, params, accumulation, accumulation_valid_mask,
                     accumulation_result_mask, gid, source,
                     uint(picks[sg][lane]));
    }
}
/*
 * Block-culled exact lookup.  A simdgroup owns an 8x8 pixel block; lane
 * L maps block positions L and L + 32 (column % 8, row / 8).  For color
 * c, far(c) bounds the distance from c to every point of the queries'
 * bounding box, so U = min far(c) bounds each query's nearest distance;
 * a color whose box distance exceeds U is strictly farther than the
 * color attaining U at every query.  Survivors keep palette order in
 * threadgroup GEMM tiles of 32 floats (rows -2r, -2g, -2b, |c|^2); a load
 * also reads the next tile's rows as rows 4..7, which meet the zero query
 * columns.  The first minimum then returns the direct scan's index, ties
 * included.  More than SIXEL_BLOCKCULL_CAP survivors use the full device
 * tiles.
 */
#define SIXEL_BLOCKCULL_CAP 128u
