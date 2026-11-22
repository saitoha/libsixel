// SPDX-License-Identifier: MIT
@group(0) @binding(1) var<storage, read> palette: array<u32>;
var<workgroup> lows: array<vec3i, 32>;
var<workgroup> highs: array<vec3i, 32>;
var<workgroup> bounds: array<u32, 32>;
var<workgroup> candidates: array<atomic<u32>, 8>;

fn block_rgb(i: u32) -> vec3i {
    let p = palette[i];
    return vec3i(i32(p & 255u), i32((p >> 8u) & 255u), i32(p >> 16u));
}

// The whole 8x8 tile shares a conservative bound. Mask bits fit all 256
// colors and are visited in palette order, including exact distance ties.
fn lookup_pair(q: array<vec4f, 2>, lane: u32) -> vec2u {
    var lo = vec3i(255);
    var hi = vec3i(0);
    for (var k = 0u; k < 2u; k++) {
        if (q[k].w > 0.0) {
            lo = min(lo, vec3i(floor(q[k].xyz)));
            hi = max(hi, vec3i(ceil(q[k].xyz)));
        }
    }
    lows[lane] = lo;
    highs[lane] = hi;
    if (lane < 8u) { atomicStore(&candidates[lane], 0u); }
    workgroupBarrier();
    for (var step = 16u; step > 0u; step /= 2u) {
        if (lane < step) {
            lows[lane] = min(lows[lane], lows[lane + step]);
            highs[lane] = max(highs[lane], highs[lane + step]);
        }
        workgroupBarrier();
    }
    lo = lows[0];
    hi = highs[0];
    var bound = 0xffffffffu;
    for (var i = lane; i < arrayLength(&palette); i += 32u) {
        let c = block_rgb(i);
        let gap = max(c - lo, hi - c);
        bound = min(bound, u32(dot(gap, gap)));
    }
    bounds[lane] = bound;
    workgroupBarrier();
    for (var step = 16u; step > 0u; step /= 2u) {
        if (lane < step) { bounds[lane] = min(bounds[lane], bounds[lane + step]); }
        workgroupBarrier();
    }
    for (var i = lane; i < arrayLength(&palette); i += 32u) {
        let c = block_rgb(i);
        let gap = max(max(lo - c, c - hi), vec3i(0));
        if (u32(dot(gap, gap)) <= bounds[0]) { atomicOr(&candidates[i / 32u], 1u << lane); }
    }
    workgroupBarrier();
    var best = vec2u(arrayLength(&palette));
    var minimum = vec2f(1e30);
    for (var k = 0u; k * 32u < arrayLength(&palette); k++) {
        var bits = atomicLoad(&candidates[k]);
        while (bits != 0u) {
            let i = k * 32u + firstTrailingBit(bits);
            let c = vec3f(block_rgb(i));
            for (var h = 0u; h < 2u; h++) {
                let gap = q[h].xyz - c;
                let d = dot(gap, gap);
                if (q[h].w > 0.0 && d < minimum[h]) { minimum[h] = d; best[h] = i; }
            }
            bits &= bits - 1u;
        }
    }
    return best;
}
