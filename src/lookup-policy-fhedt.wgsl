// SPDX-License-Identifier: MIT
// Accel: LOOKUP-FHEDT webgpu
@group(0) @binding(7) var<storage, read> config: array<u32>;

// One invocation owns a whole line. X begins with quantized palette sites.
// Later colliding registers win, then the earlier X on equally distant sites.
@compute @workgroup_size(32)
fn fhedt_x(@builtin(global_invocation_id) id: vec3u) {
    let r = 256u >> config[0];
    let line = id.x;
    if (line >= r * r) { return; }
    var labels: array<u32, 256>;
    for (var i = 0u; i < arrayLength(&palette); i++) {
        let c = vec3u(table_rgb(palette[i])) >> vec3u(config[0]);
        if (c.y + r * c.z == line) { labels[c.x] = i + 1u; }
    }
    var nearest = -1i;
    for (var x = 0u; x < r; x++) {
        if (labels[x] != 0u) { nearest = i32(x); }
        var value = 0xffffffffu;
        if (nearest >= 0) {
            let d = x - u32(nearest);
            value = ((d * d) << 8u) | (labels[u32(nearest)] - 1u);
        }
        table[line * r + x] = value;
    }
    nearest = -1;
    for (var x = i32(r) - 1; x >= 0; x--) {
        if (labels[u32(x)] != 0u) { nearest = x; }
        if (nearest >= 0) {
            let d = u32(nearest - x);
            if (d * d < (table[line * r + u32(x)] >> 8u)) {
                table[line * r + u32(x)] = ((d * d) << 8u) | (labels[u32(nearest)] - 1u);
            }
        }
    }
}

// The lower envelope of parabolas gives all squared distances on one
// axis in linear time. A later site starts where it strictly beats its
// predecessor, preserving the earlier-coordinate tie order on Y and Z.
fn edt(line: u32, axis: u32) {
    let r = 256u >> config[0];
    let cells = r * r * r;
    if (line >= r * r) { return; }
    let stride = select(r * r, r, axis == 1u);
    let base = select(line, line % r + (line / r) * r * r, axis == 1u);
    let src = select(cells, 0u, axis == 1u);
    let dst = cells - src;
    var sites: array<i32, 256>;
    var starts: array<i32, 256>;
    var k = -1i;
    for (var q = 0i; q < i32(r); q++) {
        let word = table[src + base + u32(q) * stride];
        if (word == 0xffffffffu) { continue; }
        var begin = -1000000i;
        while (k >= 0) {
            let old = sites[u32(k)];
            let old_cost = table[src + base + u32(old) * stride] >> 8u;
            let num = i32(word >> 8u) + q * q - i32(old_cost) - old * old;
            let den = 2 * (q - old);
            begin = num / den + 1 - i32(num % den < 0);
            if (begin > starts[u32(k)]) { break; }
            k--;
        }
        k++;
        sites[u32(k)] = q;
        starts[u32(k)] = select(begin, -1000000i, k == 0);
    }
    let end = k;
    k = 0;
    for (var x = 0i; x < i32(r); x++) {
        let at = dst + base + u32(x) * stride;
        if (end < 0) { table[at] = 0xffffffffu; continue; }
        while (k < end && starts[u32(k + 1)] <= x) { k++; }
        let old = sites[u32(k)];
        let word = table[src + base + u32(old) * stride];
        let cost = (word >> 8u) + u32((x - old) * (x - old));
        table[at] = (cost << 8u) | (word & 255u);
    }
}

@compute @workgroup_size(32)
fn fhedt_y(@builtin(global_invocation_id) id: vec3u) { edt(id.x, 1u); }
@compute @workgroup_size(32)
fn fhedt_z(@builtin(global_invocation_id) id: vec3u) { edt(id.x, 2u); }

// Reuse the Y pass's scratch space after Z has finished. Store the register
// in bits 0–7, the six-neighbor boundary flag in bit 8 and zero cost in bit 9.
@compute @workgroup_size(128)
fn fhedt_pack(@builtin(global_invocation_id) id: vec3u,
              @builtin(num_workgroups) groups: vec3u) {
    let r = 256u >> config[0];
    let at = id.x + id.y * (groups.x * 128u);
    let cell = vec3u(at % r, (at / r) % r, at / (r * r));
    let word = table[at];
    let label = word & 255u;
    var boundary = false;
    var stride = 1u;
    for (var a = 0u; a < 3u; a++) {
        if ((cell[a] > 0u && (table[at - stride] & 255u) != label) ||
            (cell[a] + 1u < r && (table[at + stride] & 255u) != label)) {
            boundary = true;
            break;
        }
        stride *= r;
    }
    table[config[3] + at] =
        label | (u32(boundary) << 8u) | (u32((word >> 8u) == 0u) << 9u);
}

fn fhedt_query(q: vec3f, fractional: bool) -> u32 {
    let shift = vec3u(config[0]);
    let r = 256u >> config[0];
    let cell = vec3u(clamp(q, vec3f(0), vec3f(255))) >> shift;
    let at = cell.x + r * (cell.y + r * cell.z);
    let lut = config[3];
    let word = table[lut + at];
    var best = word & 255u;
    // Zero cost is sufficient for byte queries. A fractional query must
    // also lie exactly on the seed before it can skip corner refinement.
    if (lut == 0u || (word & 256u) == 0u ||
        (config[2] != 0u && (word & 512u) != 0u &&
         (!fractional || all(q == vec3f(cell << shift))))) { return best; }
    var minimum = 1e30f;
    for (var k = 0u; k < 8u; k++) {
        let offset = vec3u(k & 1u, (k >> 1u) & 1u, k >> 2u);
        let corner = min(cell + offset, vec3u(r - 1u));
        let i = table[lut + corner.x + r * (corner.y + r * corner.z)] & 255u;
        let d = q - vec3f((vec3u(table_rgb(palette[i])) >> shift) << shift);
        let score = dot(d, d);
        if (score < minimum) { minimum = score; best = i; }
    }
    return best;
}
fn lookup(q: vec3i) -> u32 { return fhedt_query(vec3f(q), false); }
fn lookup_fractional(q: vec3f) -> u32 { return fhedt_query(q, true); }
