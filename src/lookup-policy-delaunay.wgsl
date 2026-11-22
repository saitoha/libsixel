// SPDX-License-Identifier: MIT
// Intersect the RGB cube with each bisector's axis bounds, rounding out.
// Overlapping closed cells whose bisector crosses the intersection supply
// a conservative Delaunay graph even for duplicate or coplanar entries.
@compute @workgroup_size(64)
fn delaunay_bounds(@builtin(global_invocation_id) id: vec3u) {
    if (id.x >= arrayLength(&palette)) { return; }
    let p = table_rgb(palette[id.x]);
    var lo = vec3i(0);
    var hi = vec3i(255);
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let c = table_rgb(palette[i]);
        let normal = 2 * (c - p);
        let rhs = dot(c, c) - dot(p, p);
        for (var a = 0u; a < 3u; a++) {
            let b = (a + 1u) % 3u;
            let d = (a + 2u) % 3u;
            if (normal[a] == 0) { continue; }
            let left = min(normal[b], 0) * 255 + min(normal[d], 0) * 255;
            let cut = f32(rhs - left) / f32(normal[a]);
            if (normal[a] > 0) { hi[a] = min(hi[a], i32(ceil(cut))); }
            else { lo[a] = max(lo[a], i32(floor(cut))); }
        }
    }
    for (var a = 0u; a < 3u; a++) {
        table[id.x * 14u + a] = u32(lo[a]);
        table[id.x * 14u + 3u + a] = u32(hi[a]);
    }
}
@compute @workgroup_size(64)
fn delaunay_edges(@builtin(global_invocation_id) id: vec3u) {
    if (id.x >= arrayLength(&palette)) { return; }
    let row = id.x * 14u;
    let p = table_rgb(palette[id.x]);
    var bits = 0u;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let c = table_rgb(palette[i]);
        let n = 2 * (c - p);
        var low = 0i;
        var high = 0i;
        var overlap = true;
        for (var a = 0u; a < 3u; a++) {
            let l = i32(max(table[row + a], table[i * 14u + a]));
            let h = i32(min(table[row + 3u + a], table[i * 14u + 3u + a]));
            overlap = overlap && l <= h;
            low += min(n[a] * l, n[a] * h);
            high += max(n[a] * l, n[a] * h);
        }
        let rhs = dot(c, c) - dot(p, p);
        if (i != id.x && overlap && low <= rhs && rhs <= high) { bits |= 1u << (i % 32u); }
        if (i % 32u == 31u || i + 1u == arrayLength(&palette)) {
            table[row + 6u + i / 32u] = bits;
            bits = 0u;
        }
    }
}

fn lookup_fractional(q: vec3f) -> u32 {
    var best = 0u;
    var minimum = table_score(q, best);
    for (var step = 0u; step < arrayLength(&palette); step++) {
        let start = best;
        for (var k = 0u; k * 32u < arrayLength(&palette); k++) {
            var bits = table[start * 14u + 6u + k];
            while (bits != 0u) {
                let i = k * 32u + firstTrailingBit(bits);
                let d = table_score(q, i);
                if (d < minimum || (d == minimum && i < best)) { best = i; minimum = d; }
                bits &= bits - 1u;
            }
        }
        if (start == best) { break; }
    }
    return best;
}
fn lookup(q: vec3i) -> u32 { return lookup_fractional(vec3f(q)); }
