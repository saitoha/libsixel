/* SPDX-License-Identifier: MIT */

#include "loader_builtin_pic_test_common.h"

void
edge_pic_begin(edge_writer_t *writer,
               unsigned int width,
               unsigned int height)
{
    static unsigned char const magic[4] = {
        0x53u, 0x80u, 0xf6u, 0x34u
    };
    static unsigned char const pict[4] = { 'P', 'I', 'C', 'T' };
    unsigned int index;

    index = 0u;
    edge_put_bytes(writer, magic, sizeof(magic));
    for (index = 0u; index < 84u; ++index) {
        edge_put_u8(writer, 0u);
    }
    edge_put_bytes(writer, pict, sizeof(pict));
    edge_put_u16be(writer, width);
    edge_put_u16be(writer, height);
    for (index = 0u; index < 8u; ++index) {
        edge_put_u8(writer, 0u);
    }
}

void
edge_pic_packet(edge_writer_t *writer,
                int chained,
                unsigned int type,
                unsigned int channels)
{
    edge_put_u8(writer, chained != 0 ? 1u : 0u);
    edge_put_u8(writer, 8u);
    edge_put_u8(writer, type);
    edge_put_u8(writer, channels);
}
