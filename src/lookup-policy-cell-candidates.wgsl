// SPDX-License-Identifier: MIT
// One bit per original register. Even a 256-way tie fits without overflow.
fn lookup_fractional(q: vec3f) -> u32 {
    let cell = vec3u(clamp(q, vec3f(0), vec3f(255))) >> vec3u(4);
    let row = ((cell.x << 8u) | (cell.y << 4u) | cell.z) * 8u;
    var best = 0u;
    var minimum = 1e30f;
    for (var k = 0u; k * 32u < arrayLength(&palette); k++) {
        var bits = table[row + k];
        while (bits != 0u) {
            let i = k * 32u + firstTrailingBit(bits);
            let d = table_score(q, i);
            if (d < minimum) { minimum = d; best = i; }
            bits &= bits - 1u;
        }
    }
    return best;
}

fn lookup(q: vec3i) -> u32 { return lookup_fractional(vec3f(q)); }
