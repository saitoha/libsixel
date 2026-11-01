/* Fix every recognized PSD blend key with an exact one-pixel result. */

#include <stdio.h>
#include <string.h>

#include "loader_builtin_memory_test_common.h"

typedef struct psd0158_case {
    char const *label;
    unsigned char key[4];
    unsigned char expected[3];
} psd0158_case_t;

static void
psd0158_put_u32be(edge_writer_t *writer, unsigned int value)
{
    edge_put_u8(writer, (value >> 24) & 0xffu);
    edge_put_u8(writer, (value >> 16) & 0xffu);
    edge_put_u8(writer, (value >> 8) & 0xffu);
    edge_put_u8(writer, value & 0xffu);
}

static void
psd0158_put_layer_record(edge_writer_t *writer,
                         unsigned char const key[4])
{
    static unsigned char const signature[4] = { '8', 'B', 'I', 'M' };
    int channel;

    channel = 0;
    psd0158_put_u32be(writer, 0u);
    psd0158_put_u32be(writer, 0u);
    psd0158_put_u32be(writer, 1u);
    psd0158_put_u32be(writer, 1u);
    edge_put_u16be(writer, 3u);
    for (channel = 0; channel < 3; ++channel) {
        edge_put_u16be(writer, (unsigned int)channel);
        psd0158_put_u32be(writer, 3u);
    }
    edge_put_bytes(writer, signature, sizeof(signature));
    edge_put_bytes(writer, key, 4u);
    edge_put_u8(writer, 255u);
    edge_put_u8(writer, 0u);
    edge_put_u8(writer, 0u);
    edge_put_u8(writer, 0u);
    psd0158_put_u32be(writer, 12u);
    psd0158_put_u32be(writer, 0u);
    psd0158_put_u32be(writer, 0u);
    psd0158_put_u32be(writer, 0u);
}

static size_t
psd0158_build(unsigned char *buffer,
              size_t capacity,
              unsigned char const key[4])
{
    static unsigned char const header[12] = {
        '8', 'B', 'P', 'S', 0u, 1u,
        0u, 0u, 0u, 0u, 0u, 0u
    };
    static unsigned char const normal[4] = { 'n', 'o', 'r', 'm' };
    static unsigned char const top_rgb[3] = { 200u, 60u, 40u };
    static unsigned char const bottom_rgb[3] = { 30u, 120u, 220u };
    unsigned char layer_info[256];
    edge_writer_t writer;
    edge_writer_t layer_writer;
    int channel;

    writer.buffer = buffer;
    writer.capacity = capacity;
    writer.length = 0u;
    writer.failed = 0;
    layer_writer.buffer = layer_info;
    layer_writer.capacity = sizeof(layer_info);
    layer_writer.length = 0u;
    layer_writer.failed = 0;
    channel = 0;

    edge_put_u16be(&layer_writer, 2u);
    psd0158_put_layer_record(&layer_writer, key);
    psd0158_put_layer_record(&layer_writer, normal);
    for (channel = 0; channel < 3; ++channel) {
        edge_put_u16be(&layer_writer, 0u);
        edge_put_u8(&layer_writer, top_rgb[channel]);
    }
    for (channel = 0; channel < 3; ++channel) {
        edge_put_u16be(&layer_writer, 0u);
        edge_put_u8(&layer_writer, bottom_rgb[channel]);
    }

    edge_put_bytes(&writer, header, sizeof(header));
    edge_put_u16be(&writer, 3u);
    psd0158_put_u32be(&writer, 1u);
    psd0158_put_u32be(&writer, 1u);
    edge_put_u16be(&writer, 8u);
    edge_put_u16be(&writer, 3u);
    psd0158_put_u32be(&writer, 0u);
    psd0158_put_u32be(&writer, 0u);
    psd0158_put_u32be(&writer, (unsigned int)layer_writer.length + 4u);
    psd0158_put_u32be(&writer, (unsigned int)layer_writer.length);
    edge_put_bytes(&writer, layer_info, layer_writer.length);
    edge_put_u16be(&writer, 0u);
    if (writer.failed != 0 || layer_writer.failed != 0) {
        return 0u;
    }
    return writer.length;
}

int
test_loader_0158_loader_builtin_psd_blend_modes_numeric(int argc, char **argv)
{
    static psd0158_case_t const cases[] = {
        { "Normal",       { 'n', 'o', 'r', 'm' }, { 200u, 60u, 40u } },
        { "Dissolve",     { 'd', 'i', 's', 's' }, { 200u, 60u, 40u } },
        { "Darken",       { 'd', 'a', 'r', 'k' }, { 30u, 60u, 40u } },
        { "Multiply",     { 'm', 'u', 'l', ' ' }, { 21u, 23u, 33u } },
        { "Color Burn",   { 'i', 'd', 'i', 'v' }, { 0u, 0u, 0u } },
        { "Linear Burn",  { 'l', 'b', 'r', 'n' }, { 0u, 0u, 0u } },
        { "Darker Color", { 'd', 'k', 'C', 'l' }, { 200u, 60u, 40u } },
        { "Lighten",      { 'l', 'i', 't', 'e' }, { 200u, 120u, 220u } },
        { "Screen",       { 's', 'c', 'r', 'n' }, { 201u, 130u, 221u } },
        { "Color Dodge",  { 'd', 'i', 'v', ' ' }, { 49u, 123u, 222u } },
        { "Linear Dodge", { 'l', 'd', 'd', 'g' }, { 202u, 133u, 223u } },
        { "Lighter Color",{ 'l', 'g', 'C', 'l' }, { 30u, 120u, 220u } },
        { "Overlay",      { 'o', 'v', 'e', 'r' }, { 33u, 35u, 178u } },
        { "Soft Light",   { 's', 'L', 'i', 't' }, { 37u, 63u, 191u } },
        { "Hard Light",   { 'h', 'L', 'i', 't' }, { 113u, 35u, 49u } },
        { "Vivid Light",  { 'v', 'L', 'i', 't' }, { 33u, 0u, 0u } },
        { "Linear Light", { 'l', 'L', 'i', 't' }, { 114u, 0u, 0u } },
        { "Pin Light",    { 'p', 'L', 'i', 't' }, { 110u, 85u, 58u } },
        { "Hard Mix",     { 'h', 'M', 'i', 'x' }, { 0u, 0u, 0u } },
        { "Difference",   { 'd', 'i', 'f', 'f' }, { 198u, 105u, 217u } },
        { "Exclusion",    { 's', 'm', 'u', 'd' }, { 200u, 128u, 219u } },
        { "Subtract",     { 'f', 's', 'u', 'b' }, { 0u, 105u, 217u } },
        { "Divide",       { 'f', 'd', 'i', 'v' }, { 41u, 255u, 255u } },
        { "Hue",          { 'h', 'u', 'e', ' ' }, { 220u, 59u, 30u } },
        { "Saturation",   { 's', 'a', 't', ' ' }, { 45u, 122u, 218u } },
        { "Color",        { 'c', 'o', 'l', 'r' }, { 218u, 66u, 45u } },
        { "Luminosity",   { 'l', 'u', 'm', ' ' }, { 27u, 109u, 202u } }
    };
    unsigned char psd[512];
    edge_frame_probe_t probe;
    SIXELSTATUS status;
    size_t case_index;
    size_t psd_size;
    int result;

    (void)argc;
    (void)argv;
    memset(psd, 0, sizeof(psd));
    memset(&probe, 0, sizeof(probe));
    status = SIXEL_FALSE;
    case_index = 0u;
    psd_size = 0u;
    result = 0;
    for (case_index = 0u;
         case_index < sizeof(cases) / sizeof(cases[0]);
         ++case_index) {
        psd_size = psd0158_build(psd, sizeof(psd), cases[case_index].key);
        memset(&probe, 0, sizeof(probe));
        status = SIXEL_FALSE;
        if (psd_size == 0u ||
            edge_load_buffer(cases[case_index].label,
                             psd,
                             psd_size,
                             1,
                             &probe,
                             &status) != 0 ||
            SIXEL_FAILED(status) || probe.callback_count != 1 ||
            probe.width[0] != 1 || probe.height[0] != 1 ||
            probe.pixelformat[0] != SIXEL_PIXELFORMAT_RGB888 ||
            probe.rgb_size[0] != 3u ||
            memcmp(probe.rgb[0], cases[case_index].expected, 3u) != 0) {
            fprintf(stderr,
                    "%s: status=%d callbacks=%d rgb=%u,%u,%u\n",
                    cases[case_index].label,
                    (int)status,
                    probe.callback_count,
                    probe.rgb[0][0],
                    probe.rgb[0][1],
                    probe.rgb[0][2]);
            result = 1;
        }
    }
    return result;
}
