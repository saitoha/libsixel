/* Prove that PNG gray16 samples below eight-bit increments are preserved. */

#include "loader_builtin_memory_test_common.h"

int
test_loader_0137_loader_builtin_png_gray16_sub8bit_numeric(int argc,
                                                            char **argv)
{
    static unsigned char const png[] = {
        0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au,
        0x00u, 0x00u, 0x00u, 0x0du, 0x49u, 0x48u, 0x44u, 0x52u,
        0x00u, 0x00u, 0x00u, 0x03u, 0x00u, 0x00u, 0x00u, 0x01u,
        0x10u, 0x00u, 0x00u, 0x00u, 0x00u, 0x6eu, 0x1bu, 0x97u,
        0x2bu, 0x00u, 0x00u, 0x00u, 0x0fu, 0x49u, 0x44u, 0x41u,
        0x54u, 0x78u, 0xdau, 0x63u, 0x60u, 0x60u, 0x64u, 0x64u,
        0xfcu, 0xffu, 0x0fu, 0x00u, 0x03u, 0x0fu, 0x02u, 0x01u,
        0x04u, 0x34u, 0xa6u, 0x42u, 0x00u, 0x00u, 0x00u, 0x00u,
        0x49u, 0x45u, 0x4eu, 0x44u, 0xaeu, 0x42u, 0x60u, 0x82u
    };
    edge_loader_options_t options;
    size_t const pixels[3] = { 0u, 1u, 2u };
    float const expected[9] = {
        1.0f / 65535.0f, 1.0f / 65535.0f, 1.0f / 65535.0f,
        257.0f / 65535.0f, 257.0f / 65535.0f, 257.0f / 65535.0f,
        65534.0f / 65535.0f, 65534.0f / 65535.0f,
        65534.0f / 65535.0f
    };

    (void)argc;
    (void)argv;
    edge_loader_options_init(&options);
    options.require_static = 1;
    return edge_expect_buffer_float_samples(
        "PNG gray16 sub-eight-bit",
        png,
        sizeof(png),
        &options,
        3,
        1,
        SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_GAMMA,
        pixels,
        expected,
        0.0000001f);
}
