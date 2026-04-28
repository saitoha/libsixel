/*
 * Verify builtin WebP ANIM policy boundaries through container parse/build
 * without allocating full animation canvases.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "src/fromwebp-container.h"

#define SIXEL_TEST_WEBP_BUFFER_CAPACITY 65536u
#define SIXEL_TEST_WEBP_ANMF_PAYLOAD_SIZE 24u

typedef struct sixel_test_webp_anim_limit_case {
    char const *label;
    unsigned int canvas_width;
    unsigned int canvas_height;
    unsigned int frame_count;
    sixel_webp_container_kind_t expected_kind;
    sixel_webp_anim_unsupported_reason_t expected_reason;
} sixel_test_webp_anim_limit_case_t;

static void
sixel_test_webp_write_u24le(unsigned char *dst, unsigned int value)
{
    if (dst == NULL) {
        return;
    }
    dst[0] = (unsigned char)(value & 0xffu);
    dst[1] = (unsigned char)((value >> 8) & 0xffu);
    dst[2] = (unsigned char)((value >> 16) & 0xffu);
}

static void
sixel_test_webp_write_u32le(unsigned char *dst, uint32_t value)
{
    if (dst == NULL) {
        return;
    }
    dst[0] = (unsigned char)(value & 0xffu);
    dst[1] = (unsigned char)((value >> 8) & 0xffu);
    dst[2] = (unsigned char)((value >> 16) & 0xffu);
    dst[3] = (unsigned char)((value >> 24) & 0xffu);
}

static int
sixel_test_webp_append_chunk(unsigned char *buffer,
                             size_t capacity,
                             size_t *cursor,
                             char const fourcc[4],
                             unsigned char const *payload,
                             size_t payload_size)
{
    size_t chunk_total_size;

    chunk_total_size = 0u;
    if (buffer == NULL || cursor == NULL || fourcc == NULL) {
        return 0;
    }
    if (payload_size > SIZE_MAX - 8u - (payload_size & 1u)) {
        return 0;
    }
    chunk_total_size = 8u + payload_size + (payload_size & 1u);
    if (*cursor > capacity || capacity - *cursor < chunk_total_size) {
        return 0;
    }

    memcpy(buffer + *cursor, fourcc, 4u);
    sixel_test_webp_write_u32le(buffer + *cursor + 4u, (uint32_t)payload_size);
    if (payload_size > 0u && payload != NULL) {
        memcpy(buffer + *cursor + 8u, payload, payload_size);
    }
    if ((payload_size & 1u) != 0u) {
        buffer[*cursor + 8u + payload_size] = 0u;
    }
    *cursor += chunk_total_size;
    return 1;
}

static int
sixel_test_webp_build_anim_container(unsigned char *buffer,
                                     size_t capacity,
                                     unsigned int canvas_width,
                                     unsigned int canvas_height,
                                     unsigned int frame_count,
                                     size_t *out_size)
{
    unsigned char vp8x_payload[10];
    unsigned char anim_payload[6];
    unsigned char anmf_payload[SIXEL_TEST_WEBP_ANMF_PAYLOAD_SIZE];
    size_t cursor;
    unsigned int frame_no;
    uint32_t riff_size;

    cursor = 0u;
    frame_no = 0u;
    riff_size = 0u;
    memset(vp8x_payload, 0, sizeof(vp8x_payload));
    memset(anim_payload, 0, sizeof(anim_payload));
    memset(anmf_payload, 0, sizeof(anmf_payload));
    if (buffer == NULL || out_size == NULL) {
        return 0;
    }
    if (canvas_width == 0u || canvas_height == 0u ||
        canvas_width > 16777216u || canvas_height > 16777216u) {
        return 0;
    }
    if (capacity < 12u) {
        return 0;
    }

    memcpy(buffer + cursor, "RIFF", 4u);
    cursor += 4u;
    sixel_test_webp_write_u32le(buffer + cursor, 0u);
    cursor += 4u;
    memcpy(buffer + cursor, "WEBP", 4u);
    cursor += 4u;

    vp8x_payload[0] = SIXEL_WEBP_VP8X_ANIMATION_FLAG;
    sixel_test_webp_write_u24le(vp8x_payload + 4u, canvas_width - 1u);
    sixel_test_webp_write_u24le(vp8x_payload + 7u, canvas_height - 1u);
    if (sixel_test_webp_append_chunk(buffer,
                                     capacity,
                                     &cursor,
                                     "VP8X",
                                     vp8x_payload,
                                     sizeof(vp8x_payload)) == 0) {
        return 0;
    }
    if (sixel_test_webp_append_chunk(buffer,
                                     capacity,
                                     &cursor,
                                     "ANIM",
                                     anim_payload,
                                     sizeof(anim_payload)) == 0) {
        return 0;
    }
    for (frame_no = 0u; frame_no < frame_count; ++frame_no) {
        if (sixel_test_webp_append_chunk(buffer,
                                         capacity,
                                         &cursor,
                                         "ANMF",
                                         anmf_payload,
                                         sizeof(anmf_payload)) == 0) {
            return 0;
        }
    }

    /*
     * Keep RIFF size computation width-safe on both 32-bit and 64-bit
     * hosts. The previous "UINT32_MAX + 8" form wrapped on 32-bit size_t.
     */
    if (cursor < 8u) {
        return 0;
    }
    if (cursor - 8u > (size_t)UINT32_MAX) {
        return 0;
    }
    riff_size = (uint32_t)(cursor - 8u);
    sixel_test_webp_write_u32le(buffer + 4u, riff_size);
    *out_size = cursor;
    return 1;
}

static int
sixel_test_webp_run_anim_limit_case(
    sixel_test_webp_anim_limit_case_t const *test_case)
{
    unsigned char container_data[SIXEL_TEST_WEBP_BUFFER_CAPACITY];
    size_t container_size;
    sixel_chunk_t chunk;
    sixel_webp_container_info_t info;
    sixel_webp_decode_plan_t plan;
    SIXELSTATUS status;

    container_size = 0u;
    memset(container_data, 0, sizeof(container_data));
    memset(&chunk, 0, sizeof(chunk));
    memset(&info, 0, sizeof(info));
    memset(&plan, 0, sizeof(plan));
    status = SIXEL_FALSE;
    if (test_case == NULL) {
        fprintf(stderr, "webp anim limit test: missing case definition\n");
        return 1;
    }

    if (sixel_test_webp_build_anim_container(container_data,
                                             sizeof(container_data),
                                             test_case->canvas_width,
                                             test_case->canvas_height,
                                             test_case->frame_count,
                                             &container_size) == 0) {
        fprintf(stderr,
                "webp anim limit test: build failed (%s)\n",
                test_case->label);
        return 1;
    }

    chunk.buffer = container_data;
    chunk.size = container_size;
    chunk.max_size = container_size;
    chunk.source_path = NULL;
    chunk.allocator = NULL;

    status = sixel_webp_parse_container(&chunk, &info);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "webp anim limit test: parse failed (%s, status=%d)\n",
                test_case->label,
                (int)status);
        return 1;
    }

    status = sixel_webp_build_decode_plan(&info, &plan);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr,
                "webp anim limit test: plan build failed (%s, status=%d)\n",
                test_case->label,
                (int)status);
        return 1;
    }

    if (plan.kind != test_case->expected_kind) {
        fprintf(stderr,
                "webp anim limit test: unexpected plan kind (%s, got=%d "
                "expected=%d)\n",
                test_case->label,
                (int)plan.kind,
                (int)test_case->expected_kind);
        return 1;
    }
    if (plan.anim_unsupported_reason != test_case->expected_reason) {
        fprintf(stderr,
                "webp anim limit test: unexpected unsupported reason (%s, "
                "got=%d expected=%d)\n",
                test_case->label,
                (int)plan.anim_unsupported_reason,
                (int)test_case->expected_reason);
        return 1;
    }
    return 0;
}

int
test_loader_0060_loader_builtin_webp_anim_policy_limits_plan(int argc,
                                                              char **argv)
{
    static sixel_test_webp_anim_limit_case_t const cases[] = {
        { "frame_count_1024_ok",
          16u, 16u, 1024u,
          SIXEL_WEBP_CONTAINER_KIND_ANIM_MVP,
          SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_NONE },
        { "frame_count_1025_unsup",
          16u, 16u, 1025u,
          SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM,
          SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_FRAME_LIMIT },
        { "frame_and_dimension_unsup_frame_priority",
          32768u, 1u, 1025u,
          SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM,
          SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_FRAME_LIMIT },
        { "dimension_32767_ok",
          32767u, 1u, 1u,
          SIXEL_WEBP_CONTAINER_KIND_ANIM_MVP,
          SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_NONE },
        { "dimension_32768_unsup",
          32768u, 1u, 1u,
          SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM,
          SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_DIMENSION_LIMIT },
        { "dimension_and_pixel_unsup_dimension_priority",
          32768u, 32768u, 1u,
          SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM,
          SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_DIMENSION_LIMIT },
        { "pixel_268435456_ok",
          16384u, 16384u, 1u,
          SIXEL_WEBP_CONTAINER_KIND_ANIM_MVP,
          SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_NONE },
        { "pixel_268460031_unsup",
          32767u, 8193u, 1u,
          SIXEL_WEBP_CONTAINER_KIND_UNSUPPORTED_ANIM,
          SIXEL_WEBP_ANIM_UNSUPPORTED_REASON_PIXEL_LIMIT }
    };
    size_t index;

    (void)argc;
    (void)argv;

    index = 0u;
    for (index = 0u; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        if (sixel_test_webp_run_anim_limit_case(&cases[index]) != 0) {
            return 1;
        }
    }
    return 0;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
