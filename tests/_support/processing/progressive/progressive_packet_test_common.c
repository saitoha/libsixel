/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/encoder-core-packet.h"
#include "src/sixel_decode_pixels.h"

#define PACKET_REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "packet case %d, line %d\n", mode, __LINE__); \
        goto end; \
    } \
} while (0)

static int
packet_test_run(sixel_indexed_packet_t *packet, size_t quantum,
                 size_t *total)
{
    size_t visits;
    size_t calls;
    int done;

    done = 0;
    *total = 0;
    for (calls = 0; calls < 200000 && !done; ++calls) {
        visits = 0;
        if (sixel_indexed_packet_advance(packet, quantum, &visits, &done)
                != SIXEL_OK || visits > quantum || visits > 256) {
            return 0;
        }
        *total += visits;
    }
    return done;
}

int
test_progressive_packet_case(int mode)
{
    static char const baseline[] =
        "\033P0;1q\"1;1;7;7#0;2;0;0;100#0!7~-!7@\033\\";
    sixel_allocator_t *allocator;
    sixel_indexed_packet_request_t request;
    sixel_indexed_packet_t *packet;
    sixel_indexed_packet_t *replay;
    sixel_palette_emitted_color_t colors[3];
    unsigned int palette[3] = { 0, 1, 2 };
    unsigned char indices[128 * 385];
    unsigned char paint[128 * 385];
    unsigned char expected[4];
    unsigned char *decoded;
    unsigned char *capture;
    size_t count;
    size_t i;
    size_t total;
    size_t replay_total;
    size_t capture_size;
    size_t prefix;
    size_t passes;
    unsigned int flags;
    int width;
    int height;
    int ok;

    allocator = NULL;
    packet = replay = NULL;
    decoded = capture = NULL;
    ok = 0;
    memset(&request, 0, sizeof(request));
    memset(colors, 0, sizeof(colors));
    memset(indices, 0, sizeof(indices));
    memset(paint, 0, sizeof(paint));
    colors[0].type = colors[1].type = colors[2].type = 2;
    colors[1].fields[0] = 100;
    colors[2].fields[1] = 100;
    paint[0] = paint[6] = paint[42] = paint[48] = 1;
    indices[6] = indices[42] = 1;
    request.width = request.height = 7;
    request.indices = indices;
    request.paint = paint;
    request.colors = colors;
    request.palette = palette;
    request.color_count = 3;
    request.byte_limit = 50000;
    prefix = mode == 69 ? sizeof(baseline) - 1 : 0;
    if (mode == 70) {
        colors[1].type = 1;
        colors[1].fields[0] = 120;
        colors[1].fields[1] = 50;
        colors[1].fields[2] = 100;
    } else if (mode == 74) {
        request.width = 600;
        request.height = 1;
        request.has_gri_arg_limit = 1;
        memset(paint, 1, 600);
        memset(indices, 1, 600);
    } else if (mode == 75) {
        memset(paint, 0, sizeof(paint));
    } else if (mode == 76) {
        request.width = 128;
        request.height = 385;
        memset(paint, 1, sizeof(paint));
        for (i = 0; i < sizeof(indices); ++i) {
            indices[i] = (unsigned char)(i % 3);
        }
    } else if (mode == 77) {
        request.has_8bit_control = 1;
    }
    count = (size_t)request.width * request.height;
    PACKET_REQUIRE(sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
        == SIXEL_OK);
    PACKET_REQUIRE(sixel_indexed_packet_new(&packet, &request, allocator)
        == SIXEL_OK);
    PACKET_REQUIRE(packet_test_run(packet, mode == 76 ? 10000 : 1, &total));
    if (mode == 75) {
        PACKET_REQUIRE(packet->painted == 0 && packet->bytes == 0);
        ok = 1;
        goto end;
    }
    PACKET_REQUIRE(packet->painted == (mode == 74 || mode == 76 ? count : 4));
    PACKET_REQUIRE(packet->colors == (unsigned int)(mode == 74 ? 1 :
        mode == 76 ? 3 : 2));
    PACKET_REQUIRE(packet->bytes < request.byte_limit);
    if (mode == 76) {
        PACKET_REQUIRE(packet->bytes > SIXEL_OUTPUT_PACKET_SIZE);
    }
    if (mode == 77) {
        PACKET_REQUIRE(packet->data[0] == 0x90 &&
            packet->data[packet->bytes - 1] == 0x9c);
    }
    if (mode == 72 || mode == 73) {
        if (mode == 72) {
            request.byte_limit = 1;
        }
        PACKET_REQUIRE(sixel_indexed_packet_new(&replay, &request, allocator)
            == SIXEL_OK);
        PACKET_REQUIRE(packet_test_run(replay, 10000, &replay_total));
        PACKET_REQUIRE(packet->bytes == replay->bytes && total == replay_total);
        PACKET_REQUIRE(memcmp(packet->data, replay->data,
            mode == 72 ? 1 : packet->bytes) == 0);
    }
    capture_size = prefix + packet->bytes;
    capture = malloc(capture_size + 1);
    PACKET_REQUIRE(capture != NULL);
    memcpy(capture, baseline, prefix);
    memcpy(capture + prefix, packet->data, packet->bytes);
    capture[capture_size] = 0;
    if (mode == 71) {
        PACKET_REQUIRE(strstr((char const *)capture, ";2;0;100;0") == NULL);
    } else if (mode == 74) {
        PACKET_REQUIRE(strstr((char const *)capture, "!255@!255@!90@") != NULL);
    }
    PACKET_REQUIRE(sixel_decode_sequence_mapped(capture, capture_size, NULL,
        &decoded, &width, &height, &flags, &passes, allocator) == SIXEL_OK);
    PACKET_REQUIRE(width == request.width && height == request.height &&
        passes == (mode == 69 ? 2U : 1U));
    for (i = 0; i < count; ++i) {
        memset(expected, 0, sizeof(expected));
        if (paint[i]) {
            expected[0] = indices[i] == 1 ? 255 : 0;
            expected[1] = indices[i] == 2 ? 255 : 0;
            expected[3] = 255;
        } else if (mode == 69) {
            expected[2] = expected[3] = 255;
        }
        PACKET_REQUIRE(memcmp(decoded + i * 4, expected, 4) == 0);
    }
    ok = 1;
end:
    free(capture);
    sixel_allocator_free(allocator, decoded);
    sixel_indexed_packet_free(packet);
    sixel_indexed_packet_free(replay);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
