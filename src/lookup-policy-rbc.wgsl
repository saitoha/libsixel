// SPDX-License-Identifier: MIT
// Assign colors to their nearest representative. Each cluster stores its
// bounding box and its members in original palette order.
@compute @workgroup_size(16)
fn rbc_build(@builtin(local_invocation_index) cluster: u32) {
    let n = arrayLength(&palette);
    let groups = min(n, 16u);
    if (cluster >= groups) { return; }
    var low = vec3i(255);
    var high = vec3i(0);
    var count = 0u;
    let row = cluster * 259u;
    for (var i = 0u; i < n; i++) {
        let c = table_rgb(palette[i]);
        var owner = 0u;
        var minimum = 1e30f;
        for (var g = 0u; g < groups; g++) {
            let d = table_score(vec3f(c), g * n / groups);
            if (d < minimum) { minimum = d; owner = g; }
        }
        if (owner == cluster) {
            table[row + 3u + count] = i;
            count++;
            low = min(low, c);
            high = max(high, c);
        }
    }
    table[row] = count;
    table[row + 1u] = u32(low.x | (low.y << 8u) | (low.z << 16u));
    table[row + 2u] = u32(high.x | (high.y << 8u) | (high.z << 16u));
}

fn lookup_fractional(q: vec3f) -> u32 {
    var best = 0u;
    var minimum = table_score(q, best);
    for (var g = 0u; g < min(arrayLength(&palette), 16u); g++) {
        let row = g * 259u;
        let low = vec3f(table_rgb(table[row + 1u]));
        let high = vec3f(table_rgb(table[row + 2u]));
        let gap = max(max(low - q, q - high), vec3f(0));
        if (dot(gap, gap) > minimum + 1.0) { continue; }
        for (var k = 0u; k < table[row]; k++) {
            let i = table[row + 3u + k];
            let d = table_score(q, i);
            if (d < minimum || (d == minimum && i < best)) { best = i; minimum = d; }
        }
    }
    return best;
}
fn lookup(q: vec3i) -> u32 { return lookup_fractional(vec3f(q)); }
