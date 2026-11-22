// SPDX-License-Identifier: MIT
// The least farthest-corner distance bounds the winner throughout the
// closed box, including fractional queries on its upper face.
@compute @workgroup_size(64)
fn gridlut_build(@builtin(global_invocation_id) id: vec3u) {
    let cell = id.x;
    let low = vec3i(i32(cell >> 8u), i32((cell >> 4u) & 15u), i32(cell & 15u)) * 16;
    let high = min(low + 16, vec3i(255));
    var bound = 2147483647i;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let c = table_rgb(palette[i]);
        let gap = max(c - low, high - c);
        bound = min(bound, dot(gap, gap));
    }
    for (var k = 0u; k * 32u < arrayLength(&palette); k++) {
        var bits = 0u;
        for (var j = 0u; j < 32u && k * 32u + j < arrayLength(&palette); j++) {
            let c = table_rgb(palette[k * 32u + j]);
            let gap = max(max(low - c, c - high), vec3i(0));
            if (dot(gap, gap) <= bound) { bits |= 1u << j; }
        }
        table[8u * cell + k] = bits;
    }
}
