// SPDX-License-Identifier: MIT
// Each vantage point owns a distance-sorted row, followed by 32^3 seeds.
// All 256 lanes participate in every sorting barrier, including row padding.
var<workgroup> keys: array<u32, 256>;
@compute @workgroup_size(256)
fn vptree_rows(@builtin(workgroup_id) group: vec3u,
               @builtin(local_invocation_index) lane: u32) {
    let row = group.x;
    var key = 0xffffffffu;
    if (lane < arrayLength(&palette)) {
        let gap = table_rgb(palette[row]) - table_rgb(palette[lane]);
        key = (u32(dot(gap, gap)) << 8u) | lane;
    }
    keys[lane] = key;
    workgroupBarrier();
    for (var width = 2u; width <= 256u; width *= 2u) {
        for (var step = width / 2u; step > 0u; step /= 2u) {
            let other = lane ^ step;
            if (other > lane) {
                let a = keys[lane];
                let b = keys[other];
                let asc = (lane & width) == 0u;
                keys[lane] = select(max(a, b), min(a, b), asc);
                keys[other] = select(min(a, b), max(a, b), asc);
            }
            workgroupBarrier();
        }
    }
    table[row * 256u + lane] = keys[lane];
}

@compute @workgroup_size(64)
fn vptree_seeds(@builtin(global_invocation_id) id: vec3u) {
    let cell = id.x;
    let q = (vec3i(i32(cell >> 10u), i32((cell >> 5u) & 31u), i32(cell & 31u)) * 2 + 1) * 255;
    var best = 0u;
    var minimum = 2147483647i;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let gap = q - table_rgb(palette[i]) * 64;
        let d = dot(gap, gap);
        if (d < minimum) { minimum = d; best = i; }
    }
    table[arrayLength(&palette) * 256u + cell] = best;
}

fn lookup_fractional(q: vec3f) -> u32 {
    let cell = vec3u(clamp(q, vec3f(0), vec3f(255))) >> vec3u(3);
    var best = table[arrayLength(&palette) * 256u +
        ((cell.x << 10u) | (cell.y << 5u) | cell.z)];
    let row = best * 256u;
    var minimum = table_score(q, best);
    let limit = 4.0 * minimum + 1.0;
    for (var j = 0u; j < arrayLength(&palette); j++) {
        let key = table[row + j];
        if (f32(key >> 8u) > limit) { break; }
        let i = key & 255u;
        let d = table_score(q, i);
        if (d < minimum || (d == minimum && i < best)) { best = i; minimum = d; }
    }
    return best;
}
fn lookup(q: vec3i) -> u32 { return lookup_fractional(vec3f(q)); }
