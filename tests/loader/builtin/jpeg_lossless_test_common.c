/* Build minimal lossless JPEG streams without external fixture generators. */

#include "loader_builtin_memory_test_common.h"
#include "jpeg_lossless_test_common.h"

typedef struct jpeg_lossless_bits {
    edge_writer_t *writer;
    unsigned int byte;
    int bit_count;
} jpeg_lossless_bits_t;

static void
jpeg_lossless_put_u16be(edge_writer_t *writer, unsigned int value)
{
    edge_put_u16be(writer, value);
}

static void
jpeg_lossless_put_marker(edge_writer_t *writer, unsigned int marker)
{
    edge_put_u8(writer, 0xffu);
    edge_put_u8(writer, marker);
}

static void
jpeg_lossless_put_bit(jpeg_lossless_bits_t *bits, unsigned int bit)
{
    bits->byte = (bits->byte << 1) | (bit & 1u);
    ++bits->bit_count;
    if (bits->bit_count == 8) {
        edge_put_u8(bits->writer, bits->byte);
        if (bits->byte == 0xffu) {
            edge_put_u8(bits->writer, 0u);
        }
        bits->byte = 0u;
        bits->bit_count = 0;
    }
}

static void
jpeg_lossless_put_bits(jpeg_lossless_bits_t *bits,
                       unsigned int value,
                       int bit_count)
{
    int shift;

    shift = 0;
    for (shift = bit_count - 1; shift >= 0; --shift) {
        jpeg_lossless_put_bit(bits, value >> shift);
    }
}

static void
jpeg_lossless_finish_bits(jpeg_lossless_bits_t *bits)
{
    while (bits->bit_count != 0) {
        jpeg_lossless_put_bit(bits, 1u);
    }
}

static int
jpeg_lossless_category(int difference)
{
    unsigned int magnitude;
    int category;

    magnitude = difference < 0
        ? (unsigned int)(-difference)
        : (unsigned int)difference;
    category = 0;
    while (magnitude != 0u) {
        magnitude >>= 1;
        ++category;
    }
    return category;
}

static void
jpeg_lossless_put_difference(jpeg_lossless_bits_t *bits, int difference)
{
    unsigned int amplitude;
    int category;

    amplitude = 0u;
    category = jpeg_lossless_category(difference);
    jpeg_lossless_put_bits(bits, (unsigned int)category, 4);
    if (category == 0) {
        return;
    }
    if (difference < 0) {
        amplitude = (unsigned int)((1 << category) - 1 + difference);
    } else {
        amplitude = (unsigned int)difference;
    }
    jpeg_lossless_put_bits(bits, amplitude, category);
}

static void
jpeg_lossless_put_dht(edge_writer_t *writer)
{
    int index;

    index = 0;
    jpeg_lossless_put_marker(writer, 0xc4u);
    jpeg_lossless_put_u16be(writer, 34u);
    edge_put_u8(writer, 0u);
    for (index = 0; index < 16; ++index) {
        edge_put_u8(writer, index == 3 ? 15u : 0u);
    }
    for (index = 0; index < 15; ++index) {
        edge_put_u8(writer, (unsigned int)index);
    }
}

static void
jpeg_lossless_put_sof3(edge_writer_t *writer,
                       int width,
                       int height,
                       int components)
{
    static unsigned char const rgb_ids[3] = { 'R', 'G', 'B' };
    int component;

    component = 0;
    jpeg_lossless_put_marker(writer, 0xc3u);
    jpeg_lossless_put_u16be(writer, (unsigned int)(8 + 3 * components));
    edge_put_u8(writer, 8u);
    jpeg_lossless_put_u16be(writer, (unsigned int)height);
    jpeg_lossless_put_u16be(writer, (unsigned int)width);
    edge_put_u8(writer, (unsigned int)components);
    for (component = 0; component < components; ++component) {
        edge_put_u8(writer,
                    components == 3
                        ? rgb_ids[component]
                        : (unsigned int)(component + 1));
        edge_put_u8(writer, 0x11u);
        edge_put_u8(writer, 0u);
    }
}

static void
jpeg_lossless_put_sos(edge_writer_t *writer,
                      int components,
                      int predictor,
                      int point_transform)
{
    static unsigned char const rgb_ids[3] = { 'R', 'G', 'B' };
    int component;

    component = 0;
    jpeg_lossless_put_marker(writer, 0xdau);
    jpeg_lossless_put_u16be(writer, (unsigned int)(6 + 2 * components));
    edge_put_u8(writer, (unsigned int)components);
    for (component = 0; component < components; ++component) {
        edge_put_u8(writer,
                    components == 3
                        ? rgb_ids[component]
                        : (unsigned int)(component + 1));
        edge_put_u8(writer, 0u);
    }
    edge_put_u8(writer, (unsigned int)predictor);
    edge_put_u8(writer, 0u);
    edge_put_u8(writer, (unsigned int)point_transform);
}

static int
jpeg_lossless_predict(int selector, int left, int above, int upper_left)
{
    switch (selector) {
    case 1:
        return left;
    case 2:
        return above;
    case 3:
        return upper_left;
    case 4:
        return left + above - upper_left;
    case 5:
        return left + (above - upper_left) / 2;
    case 6:
        return above + (left - upper_left) / 2;
    case 7:
        return (left + above) / 2;
    default:
        return left;
    }
}

size_t
jpeg_lossless_build_gray_predictor(unsigned char *buffer,
                                   size_t capacity,
                                   int predictor,
                                   int point_transform)
{
    static unsigned char const samples[9] = {
        128u, 160u, 112u,
        192u, 144u, 208u,
        176u, 240u, 96u
    };
    edge_writer_t writer;
    jpeg_lossless_bits_t bits;
    int x;
    int y;
    int index;
    int predicted;
    int difference;
    int divisor;

    writer.buffer = buffer;
    writer.capacity = capacity;
    writer.length = 0u;
    writer.failed = 0;
    bits.writer = &writer;
    bits.byte = 0u;
    bits.bit_count = 0;
    x = 0;
    y = 0;
    index = 0;
    predicted = 0;
    difference = 0;
    divisor = 1 << point_transform;

    jpeg_lossless_put_marker(&writer, 0xd8u);
    jpeg_lossless_put_sof3(&writer, 3, 3, 1);
    jpeg_lossless_put_dht(&writer);
    jpeg_lossless_put_sos(&writer, 1, predictor, point_transform);
    for (y = 0; y < 3; ++y) {
        for (x = 0; x < 3; ++x) {
            index = y * 3 + x;
            if (x == 0 && y == 0) {
                predicted = 128;
            } else if (y == 0) {
                predicted = samples[index - 1];
            } else if (x == 0) {
                predicted = samples[index - 3];
            } else {
                predicted = jpeg_lossless_predict(
                    predictor,
                    samples[index - 1],
                    samples[index - 3],
                    samples[index - 4]);
            }
            difference = (int)samples[index] - predicted;
            if (difference % divisor != 0) {
                writer.failed = 1;
                continue;
            }
            jpeg_lossless_put_difference(&bits, difference / divisor);
        }
    }
    jpeg_lossless_finish_bits(&bits);
    jpeg_lossless_put_marker(&writer, 0xd9u);
    return writer.failed == 0 ? writer.length : 0u;
}

size_t
jpeg_lossless_build_rgb_restart(unsigned char *buffer, size_t capacity)
{
    static unsigned char const samples[9] = {
        100u, 140u, 180u,
        220u, 60u, 16u,
        64u, 200u, 128u
    };
    edge_writer_t writer;
    jpeg_lossless_bits_t bits;
    int pixel;
    int component;

    writer.buffer = buffer;
    writer.capacity = capacity;
    writer.length = 0u;
    writer.failed = 0;
    bits.writer = &writer;
    bits.byte = 0u;
    bits.bit_count = 0;
    pixel = 0;
    component = 0;

    jpeg_lossless_put_marker(&writer, 0xd8u);
    jpeg_lossless_put_sof3(&writer, 3, 1, 3);
    jpeg_lossless_put_dht(&writer);
    jpeg_lossless_put_marker(&writer, 0xddu);
    jpeg_lossless_put_u16be(&writer, 4u);
    jpeg_lossless_put_u16be(&writer, 1u);
    jpeg_lossless_put_sos(&writer, 3, 4, 0);
    for (pixel = 0; pixel < 3; ++pixel) {
        for (component = 0; component < 3; ++component) {
            jpeg_lossless_put_difference(
                &bits,
                (int)samples[pixel * 3 + component] - 128);
        }
        if (pixel < 2) {
            jpeg_lossless_finish_bits(&bits);
            jpeg_lossless_put_marker(&writer,
                                     (unsigned int)(0xd0 + pixel));
            bits.byte = 0u;
            bits.bit_count = 0;
        }
    }
    jpeg_lossless_finish_bits(&bits);
    jpeg_lossless_put_marker(&writer, 0xd9u);
    return writer.failed == 0 ? writer.length : 0u;
}
