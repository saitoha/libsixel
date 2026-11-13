struct EytzingerEntry {
    float key;
    uint palette_index;
};
struct EytzingerParams {
    uint count;
    uint window;
};

static uint eytzinger_palette_lookup(
    constant uchar *palette,
    constant EytzingerEntry *sorted,
    constant uint *rank_lut,
    constant EytzingerParams& params,
    uchar3 q)
{
    float key;
    uint rank;
    uint best;
    uint best_dist;
    uint left;
    uint right;
    uint step;
    uint candidate;
    float key_diff;
    float bound;
    bool left_open;
    bool right_open;
    key = float(q.x) + float(q.y) + float(q.z);
    rank = rank_lut[uint(key)];
    best = sorted[rank].palette_index;
    best_dist = palette_distance(palette, q, best);
    left = rank;
    right = rank;
    left_open = true;
    right_open = true;
    for (step = 0u; step < params.window; ++step) {
        bound = float(best_dist);
        if (left_open) {
            if (left == 0u) {
                left_open = false;
            } else {
                candidate = left - 1u;
                key_diff = key - sorted[candidate].key;
                if (key_diff * key_diff <= bound) {
                    update_palette_best(palette, q,
                                        sorted[candidate].palette_index,
                                        best,
                                        best_dist);
                    left = candidate;
                } else {
                    left_open = false;
                }
            }
        }
        bound = float(best_dist);
        if (right_open) {
            if (right + 1u >= params.count) {
                right_open = false;
            } else {
                candidate = right + 1u;
                key_diff = key - sorted[candidate].key;
                if (key_diff * key_diff <= bound) {
                    update_palette_best(palette, q,
                                        sorted[candidate].palette_index,
                                        best,
                                        best_dist);
                    right = candidate;
                } else {
                    right_open = false;
                }
            }
        }
    }
    return best;
}
