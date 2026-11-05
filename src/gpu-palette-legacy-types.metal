#include <metal_stdlib>
using namespace metal;
struct Params {
    uint pixel_count;
    uint width;
    uint height;
    uint ncolors;
    uint lookup_mode;
    uint fhedt_resolution, fhedt_shift, fhedt_refine, fhedt_dist2;
    uint mode;
    uint has_transparent;
    uint transparent_size;
    uint transparent_keycolor;
    uint has_6delta;
    uint accumulation_pixel_count;
    uint accumulation_valid_mask_size;
    uint accumulation_keycolor;
    uint accumulation_stride, accumulation_origin;
    uint sixdelta_threshold;
    uint sixdelta_metric;
    uint sixdelta_compare;
    uint residual_threshold;
    uint residual_columns, residual_rows;
    uint residual_tiled;
    uint residual_cells[128];
    uint accumulation_result_mask_size;
    float positional_strength;
    float bluenoise_strength;
    float bluenoise_gradient_factor;
    int bluenoise_phase_x;
    int bluenoise_phase_y;
    uint bluenoise_channel_rgb;
    uint gradient_size;
    uint gradient_width;
    uint gradient_height;
    uchar sixdelta_palette[768];
};
struct EytzingerEntry {
    float key;
    uint palette_index;
};
struct EytzingerParams {
    uint count;
    uint window;
};
