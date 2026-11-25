/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

#include <stdlib.h>

#include "decoder-prescan.h"
#include "sixel_atomic.h"
#include "bit-scan.h"
#include "simd-config.h"

#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
# include <arm_neon.h>
#endif

/* A delimiter-dense stream can outgrow the accelerator's useful budget. */
#define SIXEL_PRESCAN_MAX_CUTS (UINT32_C(1) << 20)

struct sixel_decode_prescan {
    sixel_atomic_u32_t ref;
    uint32_t *cuts;
    size_t count;
    size_t capacity;
    size_t size;
    size_t start;
    size_t quote;
    unsigned int quotes;
    int incomplete;
};

SIXELAPI SIXELSTATUS
sixel_decode_prescan_new(sixel_decode_prescan_t **out_plan)
{
    sixel_decode_prescan_t *plan;

    if (out_plan == NULL)
        return SIXEL_BAD_ARGUMENT;
    plan = (sixel_decode_prescan_t *)calloc(1, sizeof(*plan));
    *out_plan = plan;
    if (plan == NULL)
        return SIXEL_BAD_ALLOCATION;
    plan->ref = 1u;
    return SIXEL_OK;
}

SIXELAPI void
sixel_decode_prescan_ref(sixel_decode_prescan_t *plan)
{
    if (plan != NULL)
        (void)sixel_atomic_fetch_add_u32(&plan->ref, 1u);
}

SIXELAPI void
sixel_decode_prescan_unref(sixel_decode_prescan_t *plan)
{
    if (plan == NULL || sixel_atomic_fetch_sub_u32(&plan->ref, 1u) != 1u)
        return;
    free(plan->cuts);
    free(plan);
}

static int
prescan_payload_byte(unsigned char byte)
{
    return (byte >= 0x20u && byte <= 0x7eu) ||
           (byte >= 0x08u && byte <= 0x0du);
}

static void
prescan_record_cut(sixel_decode_prescan_t *plan, size_t offset, uint32_t kind)
{
    uint32_t *cuts;
    size_t capacity;

    if (plan->count == plan->capacity) {
        if (plan->capacity == SIXEL_PRESCAN_MAX_CUTS) {
            plan->incomplete = 1;
            return;
        }
        capacity = plan->capacity == 0 ? 1024u : plan->capacity * 2u;
        cuts = (uint32_t *)realloc(plan->cuts, capacity * sizeof(*cuts));
        if (cuts == NULL) {
            plan->incomplete = 1;
            return;
        }
        plan->cuts = cuts;
        plan->capacity = capacity;
    }
    plan->cuts[plan->count++] = (uint32_t)offset |
                               (kind << SIXEL_PRESCAN_CUT_SHIFT);
    plan->start = offset + (kind != SIXEL_PRESCAN_CUT_GRAPHIC);
}

static void
prescan_record_quote(sixel_decode_prescan_t *plan, size_t offset)
{
    if (plan->quotes == 0)
        plan->quote = offset;
    plan->quotes += plan->quotes < 2;
}

#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
/* Accel: DECODE-BODY-SPLIT neon */
static inline uint64_t
prescan_lane_mask(uint8x16_t flags)
{
    return vget_lane_u64(vreinterpret_u64_u8(
        vshrn_n_u16(vreinterpretq_u16_u8(flags), 4)), 0);
}
#endif

/* Accel: DECODE-BODY-SPLIT neon */
static size_t
prescan_run(unsigned char const *data, size_t size)
{
    size_t p;
#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
    size_t i;
    uint64_t hits;
    uint8x16_t v;
    unsigned int bit;
#endif

    p = 0;
#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
    for (; size - p >= 16u; p += 16u) {
        v = vsubq_u8(vld1q_u8(data + p), vdupq_n_u8(0x20));
        hits = prescan_lane_mask(vcgtq_u8(v, vdupq_n_u8(0x5e)));
        while (hits != 0) {
            bit = sixel_bit_scan_forward64(hits) & ~3u;
            hits &= ~(UINT64_C(15) << bit);
            i = p + (bit >> 2);
            if (!prescan_payload_byte(data[i]))
                return i;
        }
    }
#endif
    for (; p < size && prescan_payload_byte(data[p]); ++p)
        ;
    return p;
}

/* Scan the terminal run and collect cuts from the same vector loads. The
 * scalar tail keeps the logical offset continuous across caller fragments. */
/* Accel: DECODE-BODY-SPLIT neon */
SIXELAPI size_t
sixel_decode_prescan_scan(sixel_decode_prescan_t *plan,
                         unsigned char const *data, size_t size)
{
    size_t p;
    size_t base;
    unsigned char ch;
#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
    size_t i;
    uint64_t hits;
    uint64_t graphics;
    uint8x16_t v;
    uint8x16_t special;
    unsigned int bit;
#endif

    if (data == NULL)
        return 0;
    if (plan == NULL || plan->incomplete)
        return prescan_run(data, size);
    if (size > SIXEL_PRESCAN_CUT_OFFSET - plan->size) {
        plan->incomplete = 1;
        return prescan_run(data, size);
    }
    base = plan->size;
    p = 0;
#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
    for (; size - p >= 16u; p += 16u) {
        v = vld1q_u8(data + p);
        special = vorrq_u8(vceqq_u8(v, vdupq_n_u8('$')),
                           vceqq_u8(v, vdupq_n_u8('-')));
        special = vorrq_u8(special, vceqq_u8(v, vdupq_n_u8('"')));
        special = vorrq_u8(special, vcgtq_u8(
            vsubq_u8(v, vdupq_n_u8(0x20)), vdupq_n_u8(0x5e)));
        hits = prescan_lane_mask(special);
        while (hits != 0) {
            bit = sixel_bit_scan_forward64(hits) & ~3u;
            hits &= ~(UINT64_C(15) << bit);
            i = p + (bit >> 2);
            ch = data[i];
            if (ch == '$' || ch == '-') {
                if (!plan->incomplete)
                    prescan_record_cut(plan, base + i, ch == '$'
                        ? SIXEL_PRESCAN_CUT_CR : SIXEL_PRESCAN_CUT_NL);
            } else if (ch == '"') {
                prescan_record_quote(plan, base + i);
            } else if (!prescan_payload_byte(ch)) {
                p = i;
                goto end;
            }
        }
        /* A SIXEL data byte consumes any pending repeat or selection. */
        if (!plan->incomplete &&
                base + p + 16u - plan->start >= SIXEL_PRESCAN_CHUNK_BYTES) {
            graphics = prescan_lane_mask(vcgeq_u8(v, vdupq_n_u8('?')));
            if (graphics != 0) {
                i = base + p +
                    (sixel_bit_scan_reverse64(graphics) >> 2);
                if (i >= plan->start)
                    prescan_record_cut(plan, i + 1u,
                                       SIXEL_PRESCAN_CUT_GRAPHIC);
            }
        }
    }
#endif
    for (; p < size; ++p) {
        ch = data[p];
        if (!prescan_payload_byte(ch))
            break;
        if (ch == '$' || ch == '-') {
            if (!plan->incomplete)
                prescan_record_cut(plan, base + p, ch == '$'
                    ? SIXEL_PRESCAN_CUT_CR : SIXEL_PRESCAN_CUT_NL);
        } else if (ch == '"') {
            prescan_record_quote(plan, base + p);
        } else if (ch >= '?' && !plan->incomplete &&
                   base + p + 1u - plan->start >= SIXEL_PRESCAN_CHUNK_BYTES) {
            prescan_record_cut(plan, base + p + 1u, SIXEL_PRESCAN_CUT_GRAPHIC);
        }
    }
#if SIXEL_SIMD_NEON_A64 && SIXEL_SIMD_LITTLE_ENDIAN
end:
#endif
    plan->size = base + p;
    return p;
}

SIXELAPI int
sixel_decode_prescan_raster(sixel_decode_prescan_t const *plan,
                           size_t payload_size, size_t *offset)
{
    if (plan == NULL || plan->incomplete || plan->size != payload_size ||
            plan->quotes != 1u || offset == NULL)
        return 0;
    *offset = plan->quote;
    return 1;
}

SIXEL_INTERNAL_API uint32_t const *
sixel_decode_prescan_cuts(sixel_decode_prescan_t const *plan,
                         size_t payload_size, size_t *count)
{
    *count = 0;
    if (plan == NULL || plan->incomplete || plan->size != payload_size)
        return NULL;
    *count = plan->count;
    return plan->cuts;
}
