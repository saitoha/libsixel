/* Byte FHEDT preserves the CPU quantized-coordinate and corner tie order.
 * F0 uses a byte table; F1 carries boundary and zero-distance flags. */
static uint fhedt_palette_lookup(constant uchar *palette,
    device const uint *storage, uchar3 query, constant Params& p)
{
    device const uchar *bytes;
    device const ushort *table;
    uint3 q, cell, corner;
    uint r, at, word, best, minimum, distance, candidate, x, y, z;
    int3 d, color;

    q = uint3(query);
    r = p.lookup_parameters[0];
    cell = q >> p.lookup_parameters[1];
    at = cell.x + cell.y * r + cell.z * r * r;
    bytes = (device const uchar *)storage;
    if (!p.lookup_parameters[2]) return uint(bytes[at]);
    table = (device const ushort *)storage;
    word = table[at];
    best = word & 255u;
    if ((word & 256u) && !(p.lookup_parameters[3] && (word & 512u))) {
        minimum = UINT_MAX;
        for (z = 0u; z < 2u; ++z) {
            for (y = 0u; y < 2u; ++y) {
                for (x = 0u; x < 2u; ++x) {
                    corner = min(cell + uint3(x, y, z), uint3(r - 1u));
                    at = corner.x + corner.y * r + corner.z * r * r;
                    candidate = table[at] & 255u;
                    color = int3(palette[candidate * 3u],
                        palette[candidate * 3u + 1u],
                        palette[candidate * 3u + 2u]);
                    color = (color >> p.lookup_parameters[1]) << p.lookup_parameters[1];
                    d = int3(q) - color;
                    distance = uint(d.x * d.x + d.y * d.y + d.z * d.z);
                    if (distance < minimum) {
                        minimum = distance;
                        best = candidate;
                    }
                }
            }
        }
    }
    return best;
}
