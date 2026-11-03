#include <metal_stdlib>
using namespace metal;
enum {
    certlut_cells = 4096,
    certlut_palette_words = 256,
    certlut_capacity = 64,
    certlut_counts = certlut_palette_words
        + certlut_cells*certlut_capacity
};
static int3 certlut_rgb(uint value)
{
    return int3(value&255u, (value>>8)&255u, (value>>16)&255u);
}
static uint certlut_distance2(int3 a, int3 b)
{
    int3 d;
    d = a-b;
    return uint(d.x*d.x+d.y*d.y+d.z*d.z);
}
kernel void certlut_pack(constant uchar *palette [[buffer(0)]],
    constant uint& count [[buffer(1)]],
    device uint *table [[buffer(3)]],
    uint at [[thread_position_in_grid]])
{
    uint i;
    if (at >= certlut_palette_words) return;
    // Repeating the last valid entry makes the final vector load safe.
    i = min(at, count-1u);
    table[at] = uint(palette[3*i]) | (uint(palette[3*i+1])<<8)
        | (uint(palette[3*i+2])<<16) | (i<<24);
}
static uint certlut_nearest(int3 query, uint count,
    device const uint *table)
{
    uint best, i;
    uint4 packed, scores;
    int4 dr, dg, db;
    best = 0xffffffffu;
    for (i = 0; i < (count+3u)/4u; ++i) {
        packed = ((device const uint4 *)table)[i];
        dr = int4(packed&255u)-query.x;
        dg = int4((packed>>8)&255u)-query.y;
        db = int4((packed>>16)&255u)-query.z;
        scores = (uint4(dr*dr+dg*dg+db*db)<<8) | (packed>>24);
        best = min(best,min(min(scores.x,scores.y),
            min(scores.z,scores.w)));
    }
    // The index breaks ties after distance, regardless of candidate order.
    return best&255u;
}
// Retain every color that can beat or tie the center winner in the cell.
// Squared-distance differences are affine, so their exact box minimum
// is attained at a corner chosen independently on each axis.
kernel void certlut_build(constant uchar *palette [[buffer(0)]],
    constant uint& count [[buffer(1)]],
    device uint *table [[buffer(3)]],
    uint cell [[thread_position_in_grid]])
{
    int3 low, high, a, b, corner;
    int margin;
    uint best, i, kept, last, row;
    (void)palette;
    if (cell >= certlut_cells) return;
    low = int3(cell>>8, (cell>>4)&15u, cell&15u)*16;
    // Closed integer bounds include 255 in the last cell.
    high = low+15;
    best = certlut_nearest(low+8,count,table);
    a = certlut_rgb(table[best]);
    row = certlut_palette_words+cell*certlut_capacity;
    kept = 0;
    last = table[best];
    for (i = 0; i < count; ++i) {
        b = certlut_rgb(table[i]);
        corner = select(high,low,a>=b);
        // Each squared RGB distance fits in 18 bits. Its signed
        // difference remains exact in int32 throughout the byte domain.
        margin = int(certlut_distance2(corner,b))
            - int(certlut_distance2(corner,a));
        if (i != best && (margin > 0 || (margin == 0 && i > best)))
            continue;
        if (kept < certlut_capacity) table[row+kept] = table[i];
        last = table[i];
        ++kept;
    }
    // Overflow never truncates candidates: zero requests a full scan.
    if (kept > certlut_capacity) {
        table[certlut_counts+cell] = 0;
        return;
    }
    table[certlut_counts+cell] = kept;
    for (i = kept; i < ((kept+3u)&~3u); ++i) table[row+i] = last;
}
static uint lookup_rgb8(uchar3 query, constant uchar *palette,
    uint count, constant uchar *configuration,
    device const uchar *workspace)
{
    device const uint *table;
    uint cell, kept, row;
    (void)palette;
    (void)configuration;
    table = (device const uint *)workspace;
    cell = (uint(query.x>>4)<<8)
        | (uint(query.y>>4)<<4) | uint(query.z>>4);
    kept = table[certlut_counts+cell];
    if (kept == 0) return certlut_nearest(int3(query),count,table);
    row = certlut_palette_words+cell*certlut_capacity;
    if (kept == 1) return table[row]>>24;
    return certlut_nearest(int3(query),kept,table+row);
}
