static uint palette_distance(
    constant uchar *palette,
    uchar3 q,
    uint palette_index)
{
    int dr = int(q.x) - int(palette[palette_index * 3u + 0u]);
    int dg = int(q.y) - int(palette[palette_index * 3u + 1u]);
    int db = int(q.z) - int(palette[palette_index * 3u + 2u]);
    return uint(dr * dr + dg * dg + db * db);
}
static uint color_distance(uchar3 left, uchar3 right)
{
    int dr = int(left.x) - int(right.x);
    int dg = int(left.y) - int(right.y);
    int db = int(left.z) - int(right.z);
    return uint(dr * dr + dg * dg + db * db);
}
static void update_palette_best(
    constant uchar *palette,
    uchar3 q,
    uint palette_index,
    thread uint& best,
    thread uint& best_dist)
{
    uint dist = palette_distance(palette, q, palette_index);
    if (dist < best_dist) {
        best_dist = dist;
        best = palette_index;
    }
}
/* Exact fixed RGB-cell candidates. Overflow keeps exhaustive lookup. */
