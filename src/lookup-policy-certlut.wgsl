// SPDX-License-Identifier: MIT
// The difference between two squared distances is affine. Its minimum
// on a closed cell is attained at the corner facing the challenger.
@compute @workgroup_size(64)
fn certlut_build(@builtin(global_invocation_id) id: vec3u) {
    let cell = id.x;
    let lo = vec3i(i32(cell >> 8u), i32((cell >> 4u) & 15u), i32(cell & 15u)) * 16;
    let hi = min(lo + 16, vec3i(255));
    var best = 0u;
    var minimum = 1e30f;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let d = table_score(vec3f(lo + 8), i);
        if (d < minimum) { minimum = d; best = i; }
    }
    let a = table_rgb(palette[best]);
    for (var k = 0u; k * 32u < arrayLength(&palette); k++) {
        var bits = 0u;
        for (var j = 0u; j < 32u && 32u * k + j < arrayLength(&palette); j++) {
            let b = table_rgb(palette[32u * k + j]);
            let corner = select(hi, lo, a >= b);
            let da = corner - a;
            let db = corner - b;
            if (dot(db, db) <= dot(da, da)) { bits |= 1u << j; }
        }
        table[8u * cell + k] = bits;
    }
}
