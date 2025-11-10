/*
 * SPDX-License-Identifier: MIT
 *
 * mediancut algorithm implementation is imported from pnmcolormap.c
 * in netpbm library.
 * http://netpbm.sourceforge.net/
 *
 * *******************************************************************************
 *                  original license block of pnmcolormap.c
 * *******************************************************************************
 *
 *   Derived from ppmquant, originally by Jef Poskanzer.
 *
 *   Copyright (C) 1989, 1991 by Jef Poskanzer.
 *   Copyright (C) 2001 by Bryan Henderson.
 *
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted, provided
 *   that the above copyright notice appear in all copies and that both that
 *   copyright notice and this permission notice appear in supporting
 *   documentation.  This software is provided "as is" without express or
 *   implied warranty.
 *
 * ******************************************************************************
 *
 * Copyright (c) 2014-2018 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "config.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

#include "status.h"
#include "compat_stub.h"
#include "allocator.h"
#include "lut.h"

#define SIXEL_LUT_BRANCH_FLAG 0x40000000U
/* #define DEBUG_LUT_TRACE 1 */

typedef struct sixel_certlut_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} sixel_certlut_color_t;

typedef struct sixel_certlut_node {
    int index;
    int left;
    int right;
    unsigned char axis;
} sixel_certlut_node_t;

typedef struct sixel_certlut {
    uint32_t *level0;
    uint8_t *pool;
    uint32_t pool_size;
    uint32_t pool_capacity;
    int wR;
    int wG;
    int wB;
    uint64_t wR2;
    uint64_t wG2;
    uint64_t wB2;
    int32_t wr_scale[256];
    int32_t wg_scale[256];
    int32_t wb_scale[256];
    int32_t *wr_palette;
    int32_t *wg_palette;
    int32_t *wb_palette;
    sixel_certlut_color_t const *palette;
    int ncolors;
    sixel_certlut_node_t *kdnodes;
    int kdnodes_count;
    int kdtree_root;
} sixel_certlut_t;

struct sixel_lut {
    int policy;
    int depth;
    int ncolors;
    int complexion;
    unsigned char const *palette;
    sixel_allocator_t *allocator;
    struct histogram_control control;
    struct cuckoo_table32 *cache;
    size_t cache_slots;
    int cache_ready;
    sixel_certlut_t cert;
    int cert_ready;
};

SIXELSTATUS
alloctupletable(tupletable *result,
                unsigned int depth,
                unsigned int size,
                sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    enum { message_buffer_size = 256 };
    char message[message_buffer_size];
    int nwrite;
    unsigned int mainTableSize;
    unsigned int tupleIntSize;
    unsigned int allocSize;
    void *pool;
    tupletable tbl;
    unsigned int i;

    if (UINT_MAX / sizeof(struct tupleint) < size) {
        nwrite = sixel_compat_snprintf(message,
                                       sizeof(message),
                                       "size %u is too big for arithmetic",
                                       size);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    mainTableSize = size * sizeof(struct tupleint *);
    if ((UINT_MAX - mainTableSize) / sizeof(struct tupleint) < size) {
        nwrite = sixel_compat_snprintf(message,
                                       sizeof(message),
                                       "size %u is too big for arithmetic",
                                       size);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    tupleIntSize = sizeof(struct tupleint) + (depth - 1U) * sizeof(sample);
    if ((UINT_MAX - mainTableSize) / tupleIntSize < size) {
        nwrite = sixel_compat_snprintf(message,
                                       sizeof(message),
                                       "size %u is too big for arithmetic",
                                       size);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    allocSize = mainTableSize + size * tupleIntSize;

    pool = sixel_allocator_malloc(allocator, allocSize);
    if (pool == NULL) {
        sixel_compat_snprintf(message,
                              sizeof(message),
                              "unable to allocate %u bytes for a %u-entry "
                              "tuple table",
                              allocSize,
                              size);
        sixel_helper_set_additional_message(message);
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    tbl = (tupletable)pool;

    for (i = 0U; i < size; ++i) {
        tbl[i] = (struct tupleint *)((char *)pool
            + mainTableSize + i * tupleIntSize);
    }

    *result = tbl;

    status = SIXEL_OK;

end:
    return status;
}


size_t
histogram_dense_size(unsigned int depth,
                     struct histogram_control const *control)
{
    size_t size;
    unsigned int exponent;
    unsigned int i;

    size = 1U;
    exponent = depth * control->channel_bits;
    for (i = 0U; i < exponent; ++i) {
        if (size > SIZE_MAX / 2U) {
            size = SIZE_MAX;
            break;
        }
        size <<= 1U;
    }

    return size;
}

struct histogram_control
histogram_control_make_for_policy(unsigned int depth, int lut_policy)
{
    struct histogram_control control;

    control.reversible_rounding = 0;
    control.channel_shift = 2U;
    if (depth > 3U) {
        control.channel_shift = 3U;
    }
    if (lut_policy == SIXEL_LUT_POLICY_5BIT) {
        control.channel_shift = 3U;
    } else if (lut_policy == SIXEL_LUT_POLICY_6BIT) {
        control.channel_shift = 2U;
        if (depth > 3U) {
            control.channel_shift = 3U;
        }
    } else if (lut_policy == SIXEL_LUT_POLICY_ROBINHOOD
               || lut_policy == SIXEL_LUT_POLICY_HOPSCOTCH) {
        control.channel_shift = 0U;
    } else if (lut_policy == SIXEL_LUT_POLICY_CERTLUT) {
        control.channel_shift = 2U;
    }
    control.channel_bits = 8U - control.channel_shift;
    control.channel_mask = (1U << control.channel_bits) - 1U;

    return control;
}

struct histogram_control
histogram_control_make(unsigned int depth)
{
    struct histogram_control control;

    control = histogram_control_make_for_policy(depth,
                                                SIXEL_LUT_POLICY_AUTO);

    return control;
}

unsigned int
histogram_reconstruct(unsigned int quantized,
                      struct histogram_control const *control)
{
    unsigned int value;

    value = quantized << control->channel_shift;
    if (quantized == control->channel_mask) {
        value = 255U;
    } else {
        if (control->channel_shift > 0U) {
            value |= (1U << (control->channel_shift - 1U));
        }
    }
    if (value > 255U) {
        value = 255U;
    }

    return value;
}

static unsigned int
histogram_quantize(unsigned int sample8,
                   struct histogram_control const *control)
{
    unsigned int quantized;
    unsigned int shift;
    unsigned int mask;
    unsigned int rounding;

    shift = control->channel_shift;
    mask = control->channel_mask;
    if (shift == 0U) {
        quantized = sample8;
    } else {
        if (control->reversible_rounding) {
            rounding = 1U << shift;
        } else {
            rounding = 1U << (shift - 1U);
        }
        quantized = (sample8 + rounding) >> shift;
        if (quantized > mask) {
            quantized = mask;
        }
    }

    return quantized;
}

unsigned int
histogram_pack_color(unsigned char const *data,
                     unsigned int depth,
                     struct histogram_control const *control)
{
    uint32_t packed;
    unsigned int n;
    unsigned int sample8;
    unsigned int bits;

    packed = 0U;
    bits = control->channel_bits;
    if (control->channel_shift == 0U) {
        for (n = 0U; n < depth; ++n) {
            packed |= (uint32_t)data[depth - 1U - n] << (n * bits);
        }
        return packed;
    }

    for (n = 0U; n < depth; ++n) {
        sample8 = (unsigned int)data[depth - 1U - n];
        packed |= histogram_quantize(sample8, control) << (n * bits);
    }

    return packed;
}

uint32_t
histogram_hash_mix(uint32_t key)
{
    uint32_t hash;

    hash = key * 0x9e3779b9U;
    hash ^= hash >> 16;
    hash *= 0x7feb352dU;
    hash ^= hash >> 15;
    hash *= 0x846ca68bU;
    hash ^= hash >> 16;
    if (hash == 0xffffffffU) {
        hash ^= 0x632be59bU;
    }

    return hash;
}

#define ROBINHOOD_INITIAL_CAPACITY 16U

struct robinhood_slot32 {
    uint32_t key;
    uint32_t color;
    uint32_t value;
    uint16_t distance;
    uint16_t pad;
};

struct robinhood_table32 {
    struct robinhood_slot32 *slots;
    size_t capacity;
    size_t count;
    sixel_allocator_t *allocator;
};

static size_t robinhood_round_capacity(size_t hint);
static SIXELSTATUS
robinhood_table32_init(struct robinhood_table32 *table,
                       size_t expected,
                       sixel_allocator_t *allocator);
static void
robinhood_table32_fini(struct robinhood_table32 *table);
static struct robinhood_slot32 *
robinhood_table32_lookup(struct robinhood_table32 *table,
                         uint32_t key,
                         uint32_t color);
static SIXELSTATUS
robinhood_table32_insert(struct robinhood_table32 *table,
                         uint32_t key,
                         uint32_t color,
                         uint32_t value);
static SIXELSTATUS
robinhood_table32_grow(struct robinhood_table32 *table);
static struct robinhood_slot32 *
robinhood_table32_place(struct robinhood_table32 *table,
                        struct robinhood_slot32 entry);

#define HOPSCOTCH_EMPTY_KEY 0xffffffffU
#define HOPSCOTCH_DEFAULT_NEIGHBORHOOD 32U
#define HOPSCOTCH_INSERT_RANGE 256U

struct hopscotch_slot32 {
    uint32_t key;
    uint32_t color;
    uint32_t value;
};

struct hopscotch_table32 {
    struct hopscotch_slot32 *slots;
    uint32_t *hopinfo;
    size_t capacity;
    size_t count;
    size_t neighborhood;
    sixel_allocator_t *allocator;
};

static SIXELSTATUS
hopscotch_table32_init(struct hopscotch_table32 *table,
                       size_t expected,
                       sixel_allocator_t *allocator);
static void
hopscotch_table32_fini(struct hopscotch_table32 *table);

static SIXELSTATUS
hopscotch_table32_insert(struct hopscotch_table32 *table,
                         uint32_t key,
                         uint32_t color,
                         uint32_t value);
static SIXELSTATUS
hopscotch_table32_grow(struct hopscotch_table32 *table);

static size_t
robinhood_round_capacity(size_t hint)
{
    size_t capacity;

    capacity = ROBINHOOD_INITIAL_CAPACITY;
    if (hint < capacity) {
        return capacity;
    }
    if (hint > SIZE_MAX / 2U) {
        capacity = SIZE_MAX;
    } else {
        capacity = hint * 2U;
    }
    if (capacity > SIZE_MAX / sizeof(struct robinhood_slot32)) {
        capacity = SIZE_MAX;
    }

    return capacity;
}

static SIXELSTATUS
robinhood_table32_init(struct robinhood_table32 *table,
                       size_t expected,
                       sixel_allocator_t *allocator)
{
    size_t hint;

    if (table == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    table->slots = NULL;
    table->capacity = 0U;
    table->count = 0U;
    table->allocator = allocator;
    if (expected < ROBINHOOD_INITIAL_CAPACITY) {
        expected = ROBINHOOD_INITIAL_CAPACITY;
    }
    if (expected > SIZE_MAX / 2U) {
        hint = SIZE_MAX / 2U;
    } else {
        hint = expected * 2U;
    }
    table->capacity = robinhood_round_capacity(hint);
    if (table->capacity == SIZE_MAX && hint != SIZE_MAX) {
        return SIXEL_BAD_ALLOCATION;
    }
    table->slots = (struct robinhood_slot32 *)
        sixel_allocator_calloc(allocator,
                               table->capacity,
                               sizeof(struct robinhood_slot32));
    if (table->slots == NULL) {
        table->capacity = 0U;
        return SIXEL_BAD_ALLOCATION;
    }

    return SIXEL_OK;
}

static void
robinhood_table32_fini(struct robinhood_table32 *table)
{
    if (table == NULL) {
        return;
    }

    if (table->allocator != NULL && table->slots != NULL) {
        sixel_allocator_free(table->allocator, table->slots);
    }
    table->slots = NULL;
    table->capacity = 0U;
    table->count = 0U;
    table->allocator = NULL;
}

static struct robinhood_slot32 *
robinhood_table32_lookup(struct robinhood_table32 *table,
                         uint32_t key,
                         uint32_t color)
{
    struct robinhood_slot32 *slot;
    size_t index;
    size_t distance;

    if (table == NULL || table->slots == NULL || table->capacity == 0U) {
        return NULL;
    }

    index = key & (table->capacity - 1U);
    slot = &table->slots[index];
    distance = 0U;
    while (slot->value != 0U) {
        if (slot->key == key && slot->color == color) {
            return slot;
        }
        if (slot->distance < distance) {
            return NULL;
        }
        index = (index + 1U) & (table->capacity - 1U);
        slot = &table->slots[index];
        distance++;
    }

    return NULL;
}

static struct robinhood_slot32 *
robinhood_table32_place(struct robinhood_table32 *table,
                        struct robinhood_slot32 entry)
{
    struct robinhood_slot32 *slot;
    struct robinhood_slot32 tmp;
    size_t index;

    if (table == NULL || table->slots == NULL || table->capacity == 0U) {
        return NULL;
    }

    index = entry.key & (table->capacity - 1U);
    slot = &table->slots[index];
    entry.distance = 0U;
    while (slot->value != 0U) {
        if (slot->distance < entry.distance) {
            tmp = *slot;
            *slot = entry;
            entry = tmp;
        }
        index = (index + 1U) & (table->capacity - 1U);
        slot = &table->slots[index];
        entry.distance++;
    }
    *slot = entry;

    return slot;
}

static SIXELSTATUS
robinhood_table32_grow(struct robinhood_table32 *table)
{
    struct robinhood_slot32 *old_slots;
    size_t old_capacity;
    size_t i;

    if (table == NULL || table->slots == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    old_slots = table->slots;
    old_capacity = table->capacity;
    table->slots = NULL;
    table->capacity = 0U;
    table->count = 0U;
    if (robinhood_table32_init(table, old_capacity * 2U,
                               table->allocator) != SIXEL_OK) {
        table->slots = old_slots;
        table->capacity = old_capacity;
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0U; i < old_capacity; ++i) {
        struct robinhood_slot32 *slot;

        slot = &old_slots[i];
        if (slot->value != 0U) {
            struct robinhood_slot32 entry;

            entry.key = slot->key;
            entry.color = slot->color;
            entry.value = slot->value;
            entry.distance = 0U;
            entry.pad = 0U;
            if (robinhood_table32_place(table, entry) == NULL) {
                sixel_allocator_free(table->allocator, table->slots);
                table->slots = old_slots;
                table->capacity = old_capacity;
                table->count = 0U;
                return SIXEL_BAD_ALLOCATION;
            }
            table->count++;
        }
    }

    sixel_allocator_free(table->allocator, old_slots);

    return SIXEL_OK;
}

static SIXELSTATUS
robinhood_table32_insert(struct robinhood_table32 *table,
                         uint32_t key,
                         uint32_t color,
                         uint32_t value)
{
    struct robinhood_slot32 entry;

    if (table == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (table->count * 2U >= table->capacity) {
        if (robinhood_table32_grow(table) != SIXEL_OK) {
            return SIXEL_BAD_ALLOCATION;
        }
    }

    entry.key = key;
    entry.color = color;
    entry.value = value;
    entry.distance = 0U;
    entry.pad = 0U;
    if (robinhood_table32_place(table, entry) == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    table->count++;

    return SIXEL_OK;
}

static SIXELSTATUS
hopscotch_table32_init(struct hopscotch_table32 *table,
                       size_t expected,
                       sixel_allocator_t *allocator)
{
    size_t hint;
    size_t capacity;
    size_t i;

    if (table == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    table->slots = NULL;
    table->hopinfo = NULL;
    table->capacity = 0U;
    table->count = 0U;
    table->neighborhood = HOPSCOTCH_DEFAULT_NEIGHBORHOOD;
    table->allocator = allocator;
    if (expected < ROBINHOOD_INITIAL_CAPACITY) {
        expected = ROBINHOOD_INITIAL_CAPACITY;
    }
    if (expected > SIZE_MAX / 2U) {
        hint = SIZE_MAX / 2U;
    } else {
        hint = expected * 2U;
    }
    capacity = robinhood_round_capacity(hint);
    if (capacity == SIZE_MAX && hint != SIZE_MAX) {
        return SIXEL_BAD_ALLOCATION;
    }
    if (capacity < table->neighborhood) {
        capacity = table->neighborhood;
    }
    table->slots = (struct hopscotch_slot32 *)
        sixel_allocator_malloc(allocator,
                               capacity * sizeof(struct hopscotch_slot32));
    if (table->slots == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    table->hopinfo = (uint32_t *)
        sixel_allocator_calloc(allocator,
                               capacity,
                               sizeof(uint32_t));
    if (table->hopinfo == NULL) {
        sixel_allocator_free(allocator, table->slots);
        table->slots = NULL;
        return SIXEL_BAD_ALLOCATION;
    }
    for (i = 0U; i < capacity; ++i) {
        table->slots[i].key = HOPSCOTCH_EMPTY_KEY;
        table->slots[i].color = 0U;
        table->slots[i].value = 0U;
    }
    table->capacity = capacity;
    table->count = 0U;
    if (table->neighborhood > 32U) {
        table->neighborhood = 32U;
    }
    if (table->neighborhood > table->capacity) {
        table->neighborhood = table->capacity;
    }

    return SIXEL_OK;
}

static void
hopscotch_table32_fini(struct hopscotch_table32 *table)
{
    sixel_allocator_t *allocator;

    if (table == NULL) {
        return;
    }

    allocator = table->allocator;
    if (allocator != NULL) {
        if (table->slots != NULL) {
            sixel_allocator_free(allocator, table->slots);
        }
        if (table->hopinfo != NULL) {
            sixel_allocator_free(allocator, table->hopinfo);
        }
    }
    table->slots = NULL;
    table->hopinfo = NULL;
    table->capacity = 0U;
    table->count = 0U;
    table->neighborhood = HOPSCOTCH_DEFAULT_NEIGHBORHOOD;
}

static struct hopscotch_slot32 *
hopscotch_table32_lookup(struct hopscotch_table32 *table,
                         uint32_t key,
                         uint32_t color)
{
    size_t index;
    size_t bit;
    size_t candidate;
    uint32_t hop;
    size_t mask;
    size_t neighborhood;

    if (table == NULL || table->slots == NULL || table->hopinfo == NULL) {
        return NULL;
    }

    mask = table->capacity - 1U;
    index = key & mask;
    hop = table->hopinfo[index];
    neighborhood = table->neighborhood;
    while (hop != 0U) {
        bit = (size_t)__builtin_ctz(hop);
        if (bit >= neighborhood) {
            break;
        }
        candidate = (index + bit) & mask;
        if (table->slots[candidate].key == key
            && table->slots[candidate].color == color) {
            return &table->slots[candidate];
        }
        hop &= hop - 1U;
    }

    return NULL;
}

static SIXELSTATUS
hopscotch_table32_grow(struct hopscotch_table32 *table)
{
    struct hopscotch_slot32 *old_slots;
    uint32_t *old_hopinfo;
    size_t old_capacity;
    size_t i;

    if (table == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    old_slots = table->slots;
    old_hopinfo = table->hopinfo;
    old_capacity = table->capacity;
    if (hopscotch_table32_init(table, old_capacity * 2U,
                               table->allocator) != SIXEL_OK) {
        table->slots = old_slots;
        table->hopinfo = old_hopinfo;
        table->capacity = old_capacity;
        return SIXEL_BAD_ALLOCATION;
    }

    for (i = 0U; i < old_capacity; ++i) {
        struct hopscotch_slot32 entry;

        entry = old_slots[i];
        if (entry.key != HOPSCOTCH_EMPTY_KEY && entry.value != 0U) {
            if (hopscotch_table32_insert(table,
                                         entry.key,
                                         entry.color,
                                         entry.value) != SIXEL_OK) {
                hopscotch_table32_fini(table);
                table->slots = old_slots;
                table->hopinfo = old_hopinfo;
                table->capacity = old_capacity;
                table->count = 0U;
                return SIXEL_BAD_ALLOCATION;
            }
        }
    }

    sixel_allocator_free(table->allocator, old_slots);
    sixel_allocator_free(table->allocator, old_hopinfo);

    return SIXEL_OK;
}

static SIXELSTATUS
hopscotch_table32_insert(struct hopscotch_table32 *table,
                         uint32_t key,
                         uint32_t color,
                         uint32_t value)
{
    size_t index;
    size_t free_index;
    size_t distance;
    size_t mask;
    size_t candidate_distance;
    size_t candidate_index;
    uint32_t hop;
    size_t bit;

    if (table == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (table->count * 2U >= table->capacity) {
        if (hopscotch_table32_grow(table) != SIXEL_OK) {
            return SIXEL_BAD_ALLOCATION;
        }
    }

    mask = table->capacity - 1U;
    index = key & mask;
    free_index = index;
    distance = 0U;
    while (distance < HOPSCOTCH_INSERT_RANGE) {
        if (table->slots[free_index].key == HOPSCOTCH_EMPTY_KEY
            || table->slots[free_index].value == 0U) {
            break;
        }
        free_index = (free_index + 1U) & mask;
        distance++;
    }
    if (distance >= HOPSCOTCH_INSERT_RANGE) {
        if (hopscotch_table32_grow(table) != SIXEL_OK) {
            return SIXEL_BAD_ALLOCATION;
        }
        return hopscotch_table32_insert(table, key, color, value);
    }

    while (distance >= table->neighborhood) {
        hop = table->hopinfo[free_index];
        candidate_distance = 0U;
        bit = 0U;
        while (candidate_distance < table->neighborhood) {
            if ((hop & (1U << bit)) != 0U) {
                candidate_index = (free_index - bit) & mask;
                table->slots[free_index] = table->slots[candidate_index];
                table->hopinfo[candidate_index] |= 1U
                    << ((bit + distance) & (table->neighborhood - 1U));
                table->hopinfo[candidate_index] &= ~(1U << bit);
                free_index = candidate_index;
                distance -= bit;
                break;
            }
            candidate_distance++;
            bit++;
        }
        if (candidate_distance >= table->neighborhood) {
            if (hopscotch_table32_grow(table) != SIXEL_OK) {
                return SIXEL_BAD_ALLOCATION;
            }
            return hopscotch_table32_insert(table, key, color, value);
        }
    }

    table->slots[free_index].key = key;
    table->slots[free_index].color = color;
    table->slots[free_index].value = value;
    table->hopinfo[index] |= 1U << distance;
    table->count++;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_lut_histogram_robinhood(unsigned char const *data,
                              unsigned int length,
                              unsigned long depth,
                              unsigned int step,
                              unsigned int max_sample,
                              tupletable2 *colorfreqtable,
                              struct histogram_control const *control,
                              int use_reversible,
                              sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    struct robinhood_table32 table;
    size_t expected;
    size_t cap_limit;
    size_t index;
    unsigned int depth_u;
    unsigned int i;
    unsigned int n;
    uint32_t bucket_color;
    uint32_t bucket_hash;
    uint32_t entry_color;
    struct robinhood_slot32 *slot;
    unsigned int component;
    unsigned int plane;
    unsigned int reconstructed;
    unsigned char reversible_pixel[4];

    status = SIXEL_FALSE;
    table.slots = NULL;
    table.capacity = 0U;
    table.count = 0U;
    table.allocator = allocator;
    cap_limit = (size_t)1U << 20;
    expected = max_sample;
    if (expected < 256U) {
        expected = 256U;
    }
    if (expected > cap_limit) {
        expected = cap_limit;
    }

    status = robinhood_table32_init(&table, expected, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "unable to allocate robinhood histogram.");
        goto cleanup;
    }

    depth_u = (unsigned int)depth;
    for (i = 0U; i < length; i += step) {
        if (use_reversible) {
            for (plane = 0U; plane < depth_u; ++plane) {
                reversible_pixel[plane]
                    = sixel_palette_reversible_value(data[i + plane]);
            }
            bucket_color = histogram_pack_color(reversible_pixel,
                                                depth_u, control);
        } else {
            bucket_color = histogram_pack_color(data + i,
                                                depth_u, control);
        }
        bucket_hash = histogram_hash_mix(bucket_color);
        slot = robinhood_table32_lookup(&table, bucket_hash, bucket_color);
        if (slot == NULL) {
            status = robinhood_table32_insert(&table,
                                              bucket_hash,
                                              bucket_color,
                                              1U);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "unable to grow robinhood histogram.");
                goto cleanup;
            }
        } else if (slot->value < UINT32_MAX) {
            slot->value++;
        }
    }

    if (table.count > UINT_MAX) {
        sixel_helper_set_additional_message(
            "too many unique colors for histogram.");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    colorfreqtable->size = (unsigned int)table.count;
    status = alloctupletable(&colorfreqtable->table,
                             depth_u,
                             (unsigned int)table.count,
                             allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    index = 0U;
    for (i = 0U; i < table.capacity; ++i) {
        slot = &table.slots[i];
        if (slot->value == 0U) {
            continue;
        }
        if (index >= colorfreqtable->size) {
            break;
        }
        entry_color = slot->color;
        colorfreqtable->table[index]->value = slot->value;
        for (n = 0U; n < depth_u; ++n) {
            component = (unsigned int)
                ((entry_color >> (n * control->channel_bits))
                 & control->channel_mask);
            reconstructed = histogram_reconstruct(component, control);
            if (use_reversible) {
                reconstructed =
                    (unsigned int)sixel_palette_reversible_value(
                        reconstructed);
            }
            colorfreqtable->table[index]->tuple[depth_u - 1U - n]
                = (sample)reconstructed;
        }
        index++;
    }

    status = SIXEL_OK;

cleanup:
    robinhood_table32_fini(&table);

    return status;
}

static SIXELSTATUS
sixel_lut_histogram_hopscotch(unsigned char const *data,
                              unsigned int length,
                              unsigned long depth,
                              unsigned int step,
                              unsigned int max_sample,
                              tupletable2 *colorfreqtable,
                              struct histogram_control const *control,
                              int use_reversible,
                              sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    struct hopscotch_table32 table;
    size_t expected;
    size_t cap_limit;
    size_t index;
    unsigned int depth_u;
    unsigned int i;
    unsigned int n;
    uint32_t bucket_color;
    uint32_t bucket_hash;
    uint32_t entry_color;
    struct hopscotch_slot32 *slot;
    unsigned int component;
    unsigned int reconstructed;
    unsigned char reversible_pixel[4];
    unsigned int plane;

    status = SIXEL_FALSE;
    table.slots = NULL;
    table.hopinfo = NULL;
    table.capacity = 0U;
    table.count = 0U;
    table.neighborhood = HOPSCOTCH_DEFAULT_NEIGHBORHOOD;
    table.allocator = allocator;
    cap_limit = (size_t)1U << 20;
    expected = max_sample;
    if (expected < 256U) {
        expected = 256U;
    }
    if (expected > cap_limit) {
        expected = cap_limit;
    }

    status = hopscotch_table32_init(&table, expected, allocator);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "unable to allocate hopscotch histogram.");
        goto cleanup;
    }

    depth_u = (unsigned int)depth;
    for (i = 0U; i < length; i += step) {
        if (use_reversible) {
            for (plane = 0U; plane < depth_u; ++plane) {
                reversible_pixel[plane]
                    = sixel_palette_reversible_value(data[i + plane]);
            }
            bucket_color = histogram_pack_color(reversible_pixel,
                                                depth_u, control);
        } else {
            bucket_color = histogram_pack_color(data + i,
                                                depth_u, control);
        }
        bucket_hash = histogram_hash_mix(bucket_color);
        slot = hopscotch_table32_lookup(&table, bucket_hash, bucket_color);
        if (slot == NULL) {
            status = hopscotch_table32_insert(&table,
                                              bucket_hash,
                                              bucket_color,
                                              1U);
            if (SIXEL_FAILED(status)) {
                sixel_helper_set_additional_message(
                    "unable to grow hopscotch histogram.");
                goto cleanup;
            }
        } else if (slot->value < UINT32_MAX) {
            slot->value++;
        }
    }

    if (table.count > UINT_MAX) {
        sixel_helper_set_additional_message(
            "too many unique colors for histogram.");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    colorfreqtable->size = (unsigned int)table.count;
    status = alloctupletable(&colorfreqtable->table,
                             depth_u,
                             (unsigned int)table.count,
                             allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    index = 0U;
    for (i = 0U; i < table.capacity; ++i) {
        slot = &table.slots[i];
        if (slot->key == HOPSCOTCH_EMPTY_KEY || slot->value == 0U) {
            continue;
        }
        if (index >= colorfreqtable->size) {
            break;
        }
        entry_color = slot->color;
        colorfreqtable->table[index]->value = slot->value;
        for (n = 0U; n < depth_u; ++n) {
            component = (unsigned int)
                ((entry_color >> (n * control->channel_bits))
                 & control->channel_mask);
            reconstructed = histogram_reconstruct(component, control);
            if (use_reversible) {
                reconstructed =
                    (unsigned int)sixel_palette_reversible_value(
                        reconstructed);
            }
            colorfreqtable->table[index]->tuple[depth_u - 1U - n]
                = (sample)reconstructed;
        }
        index++;
    }

    status = SIXEL_OK;

cleanup:
    hopscotch_table32_fini(&table);

    return status;
}

SIXELSTATUS
sixel_lut_build_histogram(unsigned char const *data,
                          unsigned int length,
                          unsigned long depth,
                          int quality_mode,
                          int use_reversible,
                          int policy,
                          tupletable2 *colorfreqtable,
                          sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    typedef uint32_t unit_t;
    unit_t *histogram;
    unit_t *refmap;
    unit_t *ref;
    unsigned int bucket_index;
    unsigned int step;
    unsigned int max_sample;
    size_t hist_size;
    unit_t bucket_value;
    unsigned int component;
    unsigned int reconstructed;
    struct histogram_control control;
    unsigned int depth_u;
    unsigned char reversible_pixel[4];
    unsigned int i;
    unsigned int n;
    unsigned int plane;

    status = SIXEL_FALSE;
    histogram = NULL;
    refmap = NULL;
    if (colorfreqtable == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    colorfreqtable->size = 0U;
    colorfreqtable->table = NULL;

    switch (quality_mode) {
    case SIXEL_QUALITY_LOW:
        max_sample = 18383U;
        break;
    case SIXEL_QUALITY_HIGH:
        max_sample = 1118383U;
        break;
    case SIXEL_QUALITY_FULL:
    default:
        max_sample = 4003079U;
        break;
    }

    if (depth == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    step = (unsigned int)(length / depth / max_sample * depth);
    if (step == 0U) {
        step = (unsigned int)depth;
    }

    sixel_debugf("making histogram...");

    depth_u = (unsigned int)depth;
    control = histogram_control_make_for_policy(depth_u, policy);
    if (use_reversible) {
        control.reversible_rounding = 1;
    }
    if (policy == SIXEL_LUT_POLICY_ROBINHOOD
        || policy == SIXEL_LUT_POLICY_HOPSCOTCH) {
        if (policy == SIXEL_LUT_POLICY_ROBINHOOD) {
            return sixel_lut_histogram_robinhood(data,
                                                 length,
                                                 depth,
                                                 step,
                                                 max_sample,
                                                 colorfreqtable,
                                                 &control,
                                                 use_reversible,
                                                 allocator);
        }
        return sixel_lut_histogram_hopscotch(data,
                                             length,
                                             depth,
                                             step,
                                             max_sample,
                                             colorfreqtable,
                                             &control,
                                             use_reversible,
                                             allocator);
    }

    hist_size = histogram_dense_size(depth_u, &control);
    histogram = (unit_t *)sixel_allocator_calloc(allocator,
                                                 hist_size,
                                                 sizeof(unit_t));
    if (histogram == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for histogram.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }
    ref = refmap = (unit_t *)sixel_allocator_malloc(allocator,
                                                    hist_size
                                                    * sizeof(unit_t));
    if (refmap == NULL) {
        sixel_helper_set_additional_message(
            "unable to allocate memory for lookup table.");
        status = SIXEL_BAD_ALLOCATION;
        goto cleanup;
    }

    for (i = 0U; i < length; i += step) {
        if (use_reversible) {
            for (plane = 0U; plane < depth_u; ++plane) {
                reversible_pixel[plane]
                    = sixel_palette_reversible_value(data[i + plane]);
            }
            bucket_index = histogram_pack_color(reversible_pixel,
                                                depth_u,
                                                &control);
        } else {
            bucket_index = histogram_pack_color(data + i,
                                                depth_u,
                                                &control);
        }
        if (histogram[bucket_index] == 0U) {
            *ref++ = bucket_index;
        }
        if (histogram[bucket_index] < UINT32_MAX) {
            histogram[bucket_index]++;
        }
    }

    colorfreqtable->size = (unsigned int)(ref - refmap);
    status = alloctupletable(&colorfreqtable->table,
                             depth,
                             (unsigned int)(ref - refmap),
                             allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    for (i = 0U; i < colorfreqtable->size; ++i) {
        bucket_value = refmap[i];
        if (histogram[bucket_value] > 0U) {
            colorfreqtable->table[i]->value = histogram[bucket_value];
            for (n = 0U; n < depth; ++n) {
                component = (unsigned int)
                    ((bucket_value >> (n * control.channel_bits))
                     & control.channel_mask);
                reconstructed = histogram_reconstruct(component,
                                                      &control);
                if (use_reversible) {
                    reconstructed =
                        (unsigned int)sixel_palette_reversible_value(
                            reconstructed);
                }
                colorfreqtable->table[i]->tuple[depth - 1U - n]
                    = (sample)reconstructed;
            }
        }
    }

    sixel_debugf("%u colors found", colorfreqtable->size);
    status = SIXEL_OK;

cleanup:
    sixel_allocator_free(allocator, refmap);
    sixel_allocator_free(allocator, histogram);

    return status;
}

static unsigned int
computeHash(unsigned char const *data,
            unsigned int depth,
            struct histogram_control const *control)
{
    uint32_t packed;

    packed = histogram_pack_color(data, depth, control);

    return histogram_hash_mix(packed);
}

#define CUCKOO_BUCKET_SIZE 4U
#define CUCKOO_MAX_KICKS 128U
#define CUCKOO_STASH_SIZE 32U
#define CUCKOO_EMPTY_KEY 0xffffffffU

struct cuckoo_bucket32 {
    uint32_t key[CUCKOO_BUCKET_SIZE];
    uint32_t value[CUCKOO_BUCKET_SIZE];
};

struct cuckoo_table32 {
    struct cuckoo_bucket32 *buckets;
    uint32_t stash_key[CUCKOO_STASH_SIZE];
    uint32_t stash_value[CUCKOO_STASH_SIZE];
    size_t bucket_count;
    size_t bucket_mask;
    size_t stash_count;
    sixel_allocator_t *allocator;
};

static size_t
cuckoo_round_buckets(size_t hint)
{
    size_t count;

    count = 1U;
    if (hint == 0U) {
        return count;
    }
    count = hint - 1U;
    count |= count >> 1;
    count |= count >> 2;
    count |= count >> 4;
    count |= count >> 8;
    count |= count >> 16;
#if SIZE_MAX > UINT32_MAX
    count |= count >> 32;
#endif
    if (count == SIZE_MAX) {
        return SIZE_MAX;
    }
    count++;
    if (count < 1U) {
        count = 1U;
    }

    return count;
}

static SIXELSTATUS
cuckoo_table32_init(struct cuckoo_table32 *table,
                    size_t expected,
                    sixel_allocator_t *allocator)
{
    size_t buckets;
    size_t i;

    if (table == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    table->buckets = NULL;
    table->bucket_count = 0U;
    table->bucket_mask = 0U;
    table->stash_count = 0U;
    table->allocator = allocator;

    buckets = cuckoo_round_buckets(expected / CUCKOO_BUCKET_SIZE);
    if (buckets == SIZE_MAX && expected != SIZE_MAX) {
        return SIXEL_BAD_ALLOCATION;
    }
    table->buckets = (struct cuckoo_bucket32 *)
        sixel_allocator_calloc(allocator,
                               buckets,
                               sizeof(struct cuckoo_bucket32));
    if (table->buckets == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    table->bucket_count = buckets;
    table->bucket_mask = buckets - 1U;

    for (i = 0U; i < buckets; ++i) {
        size_t j;

        for (j = 0U; j < CUCKOO_BUCKET_SIZE; ++j) {
            table->buckets[i].key[j] = CUCKOO_EMPTY_KEY;
            table->buckets[i].value[j] = 0U;
        }
    }

    for (i = 0U; i < CUCKOO_STASH_SIZE; ++i) {
        table->stash_key[i] = CUCKOO_EMPTY_KEY;
        table->stash_value[i] = 0U;
    }

    table->stash_count = 0U;

    return SIXEL_OK;
}

static void
cuckoo_table32_clear(struct cuckoo_table32 *table)
{
    size_t i;

    if (table == NULL) {
        return;
    }

    for (i = 0U; i < table->bucket_count; ++i) {
        size_t j;

        for (j = 0U; j < CUCKOO_BUCKET_SIZE; ++j) {
            table->buckets[i].key[j] = CUCKOO_EMPTY_KEY;
            table->buckets[i].value[j] = 0U;
        }
    }

    for (i = 0U; i < CUCKOO_STASH_SIZE; ++i) {
        table->stash_key[i] = CUCKOO_EMPTY_KEY;
        table->stash_value[i] = 0U;
    }
    table->stash_count = 0U;
}

static void
cuckoo_table32_fini(struct cuckoo_table32 *table)
{
    if (table == NULL) {
        return;
    }

    sixel_allocator_free(table->allocator, table->buckets);
    table->buckets = NULL;
    table->bucket_count = 0U;
    table->bucket_mask = 0U;
    table->stash_count = 0U;
}

static uint32_t *
cuckoo_table32_lookup(struct cuckoo_table32 *table, uint32_t key)
{
    size_t first_index;
    size_t second_index;
    size_t i;

    if (table == NULL || key == CUCKOO_EMPTY_KEY) {
        return NULL;
    }
    if (table->bucket_count == 0U) {
        return NULL;
    }

    first_index = key & table->bucket_mask;
    second_index = histogram_hash_mix(key ^ 0x9e3779b9U)
                 & table->bucket_mask;

    for (i = 0U; i < CUCKOO_BUCKET_SIZE; ++i) {
        if (table->buckets[first_index].key[i] == key) {
            return &table->buckets[first_index].value[i];
        }
    }
    for (i = 0U; i < CUCKOO_BUCKET_SIZE; ++i) {
        if (table->buckets[second_index].key[i] == key) {
            return &table->buckets[second_index].value[i];
        }
    }
    for (i = 0U; i < table->stash_count; ++i) {
        if (table->stash_key[i] == key) {
            return &table->stash_value[i];
        }
    }

    return NULL;
}

static SIXELSTATUS
cuckoo_table32_grow(struct cuckoo_table32 *table)
{
    struct cuckoo_table32 tmp;
    size_t new_buckets;
    size_t i;
    SIXELSTATUS status;

    if (table == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    new_buckets = table->bucket_count * 2U;
    if (new_buckets < table->bucket_count) {
        return SIXEL_BAD_ALLOCATION;
    }
    memset(&tmp, 0, sizeof(tmp));
    status = cuckoo_table32_init(&tmp,
                                 new_buckets * CUCKOO_BUCKET_SIZE,
                                 table->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    for (i = 0U; i < table->bucket_count; ++i) {
        size_t j;

        for (j = 0U; j < CUCKOO_BUCKET_SIZE; ++j) {
            uint32_t key;
            uint32_t value;
            size_t index;

            key = table->buckets[i].key[j];
            value = table->buckets[i].value[j];
            if (key == CUCKOO_EMPTY_KEY || value == 0U) {
                continue;
            }
            index = key & tmp.bucket_mask;
            tmp.buckets[index].key[0] = key;
            tmp.buckets[index].value[0] = value;
        }
    }

    for (i = 0U; i < table->stash_count; ++i) {
        size_t index;

        index = table->stash_key[i] & tmp.bucket_mask;
        tmp.buckets[index].key[0] = table->stash_key[i];
        tmp.buckets[index].value[0] = table->stash_value[i];
    }

    cuckoo_table32_fini(table);
    *table = tmp;

    return SIXEL_OK;
}

static SIXELSTATUS
cuckoo_table32_insert(struct cuckoo_table32 *table,
                      uint32_t key,
                      uint32_t value)
{
    size_t index;
    size_t i;
    uint32_t cur_key;
    uint32_t cur_value;
    unsigned int kick;
    SIXELSTATUS status;

    if (table == NULL || key == CUCKOO_EMPTY_KEY) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (value == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (table->bucket_count == 0U) {
        return SIXEL_BAD_ARGUMENT;
    }

    index = key & table->bucket_mask;
    for (i = 0U; i < CUCKOO_BUCKET_SIZE; ++i) {
        if (table->buckets[index].value[i] == 0U) {
            table->buckets[index].key[i] = key;
            table->buckets[index].value[i] = value;
            return SIXEL_OK;
        }
    }

    index = histogram_hash_mix(key ^ 0x9e3779b9U) & table->bucket_mask;
    for (i = 0U; i < CUCKOO_BUCKET_SIZE; ++i) {
        if (table->buckets[index].value[i] == 0U) {
            table->buckets[index].key[i] = key;
            table->buckets[index].value[i] = value;
            return SIXEL_OK;
        }
    }

    cur_key = key;
    cur_value = value;
    for (kick = 0U; kick < CUCKOO_MAX_KICKS; ++kick) {
        size_t slot;

        index = cur_key & table->bucket_mask;
        slot = kick & (CUCKOO_BUCKET_SIZE - 1U);
        key = table->buckets[index].key[slot];
        table->buckets[index].key[slot] = cur_key;
        cur_key = key;
        key = table->buckets[index].value[slot];
        table->buckets[index].value[slot] = cur_value;
        cur_value = key;
        if (cur_value == 0U) {
            return SIXEL_OK;
        }

        index = histogram_hash_mix(cur_key ^ 0x9e3779b9U)
              & table->bucket_mask;
        slot = (kick + 1U) & (CUCKOO_BUCKET_SIZE - 1U);
        key = table->buckets[index].key[slot];
        table->buckets[index].key[slot] = cur_key;
        cur_key = key;
        key = table->buckets[index].value[slot];
        table->buckets[index].value[slot] = cur_value;
        cur_value = key;
        if (cur_value == 0U) {
            return SIXEL_OK;
        }
    }

    if (table->stash_count >= CUCKOO_STASH_SIZE) {
        status = cuckoo_table32_grow(table);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        return cuckoo_table32_insert(table, cur_key, cur_value);
    }

    table->stash_key[table->stash_count] = cur_key;
    table->stash_value[table->stash_count] = cur_value;
    table->stash_count++;

    return SIXEL_OK;
}

static int
sixel_lut_policy_normalize(int policy)
{
    int normalized;

    normalized = policy;
    if (normalized == SIXEL_LUT_POLICY_AUTO) {
        normalized = SIXEL_LUT_POLICY_6BIT;
    }

    return normalized;
}

static int
sixel_lut_policy_uses_cache(int policy)
{
    if (policy == SIXEL_LUT_POLICY_CERTLUT) {
        return 0;
    }

    return 1;
}

static void
sixel_lut_release_cache(sixel_lut_t *lut)
{
    if (lut == NULL || lut->cache == NULL) {
        return;
    }

    cuckoo_table32_fini(lut->cache);
    sixel_allocator_free(lut->allocator, lut->cache);
    lut->cache = NULL;
    lut->cache_slots = 0U;
    lut->cache_ready = 0;
}

static SIXELSTATUS
sixel_lut_prepare_cache(sixel_lut_t *lut)
{
    SIXELSTATUS status;
    struct cuckoo_table32 *table;
    size_t expected;
    size_t cap_limit;

    if (lut == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (!sixel_lut_policy_uses_cache(lut->policy)) {
        return SIXEL_OK;
    }
    if (lut->allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (lut->control.channel_shift == 0U) {
        expected = (size_t)lut->ncolors * 64U;
        cap_limit = (size_t)1U << 20;
        if (expected < 512U) {
            expected = 512U;
        }
        if (expected > cap_limit) {
            expected = cap_limit;
        }
    } else {
        expected = histogram_dense_size((unsigned int)lut->depth,
                                        &lut->control);
        if (expected == 0U) {
            expected = CUCKOO_BUCKET_SIZE;
        }
    }

    table = lut->cache;
    if (table != NULL) {
        if (table->bucket_count * CUCKOO_BUCKET_SIZE < expected) {
            sixel_lut_release_cache(lut);
            table = NULL;
        } else {
            cuckoo_table32_clear(table);
        }
    }
    if (table == NULL) {
        table = (struct cuckoo_table32 *)
            sixel_allocator_malloc(lut->allocator,
                                   sizeof(struct cuckoo_table32));
        if (table == NULL) {
            sixel_helper_set_additional_message(
                "sixel_lut_prepare_cache: allocation failed.");
            return SIXEL_BAD_ALLOCATION;
        }
        memset(table, 0, sizeof(struct cuckoo_table32));
        status = cuckoo_table32_init(table, expected, lut->allocator);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(lut->allocator, table);
            sixel_helper_set_additional_message(
                "sixel_lut_prepare_cache: unable to init cache table.");
            return status;
        }
        lut->cache = table;
    }

    lut->cache_slots = expected;
    lut->cache_ready = 1;

    return SIXEL_OK;
}

static int
sixel_lut_lookup_fast(sixel_lut_t *lut, unsigned char const *pixel)
{
    int result;
    unsigned int hash;
    int diff;
    int i;
    int distant;
    struct cuckoo_table32 *table;
    uint32_t *slot;
    SIXELSTATUS status;
    unsigned char const *entry;
    unsigned char const *end;
    int pixel0;
    int pixel1;
    int pixel2;
    int delta;

    if (lut == NULL || pixel == NULL) {
        return 0;
    }
    if (lut->palette == NULL || lut->ncolors <= 0) {
        return 0;
    }

    result = (-1);
    diff = INT_MAX;
    hash = computeHash(pixel, (unsigned int)lut->depth, &lut->control);

    table = lut->cache;
    if (table != NULL) {
        slot = cuckoo_table32_lookup(table, hash);
        if (slot != NULL && *slot != 0U) {
            return (int)(*slot - 1U);
        }
    }

    entry = lut->palette;
    end = lut->palette + (size_t)lut->ncolors * (size_t)lut->depth;
    pixel0 = (int)pixel[0];
    pixel1 = (int)pixel[1];
    pixel2 = (int)pixel[2];
    for (i = 0; entry < end; ++i, entry += lut->depth) {
        delta = pixel0 - (int)entry[0];
        distant = delta * delta * lut->complexion;
        delta = pixel1 - (int)entry[1];
        distant += delta * delta;
        delta = pixel2 - (int)entry[2];
        distant += delta * delta;
        if (distant < diff) {
            diff = distant;
            result = i;
        }
    }

    if (table != NULL && result >= 0) {
        status = cuckoo_table32_insert(table,
                                       hash,
                                       (uint32_t)(result + 1));
        if (SIXEL_FAILED(status)) {
        }
    }

    if (result < 0) {
        result = 0;
    }

    return result;
}

static void sixel_certlut_cell_center(int rmin, int gmin, int bmin, int size,
                                      int *cr, int *cg, int *cb);
static void sixel_certlut_weight_init(sixel_certlut_t *lut,
                                      int wR,
                                      int wG,
                                      int wB);
static uint64_t sixel_certlut_distance_precomputed(
    sixel_certlut_t const *lut,
    int index,
    int32_t wr_r,
    int32_t wg_g,
    int32_t wb_b);
static int sixel_certlut_palette_component(sixel_certlut_t const *lut,
                                           int index,
                                           int axis);
static void sixel_certlut_sort_indices(sixel_certlut_t const *lut,
                                       int *indices,
                                   int count,
                                   int axis);
static SIXELSTATUS sixel_certlut_kdtree_build(sixel_certlut_t *lut);
static int sixel_certlut_kdtree_build_recursive(sixel_certlut_t *lut,
                                            int *indices,
                                            int count,
                                            int depth);
static uint64_t sixel_certlut_axis_distance(sixel_certlut_t const *lut,
                                        int diff,
                                        int axis);
static void sixel_certlut_consider_candidate(sixel_certlut_t const *lut,
                                         int candidate,
                                         int32_t wr_r,
                                         int32_t wg_g,
                                         int32_t wb_b,
                                         int *best_idx,
                                         uint64_t *best_dist,
                                         int *second_idx,
                                         uint64_t *second_dist);
static void sixel_certlut_kdtree_search(sixel_certlut_t const *lut,
                                    int node_index,
                                    int r,
                                    int g,
                                    int b,
                                    int32_t wr_r,
                                    int32_t wg_g,
                                    int32_t wb_b,
                                    int *best_idx,
                                    uint64_t *best_dist,
                                    int *second_idx,
                                    uint64_t *second_dist);
static void sixel_certlut_distance_pair(sixel_certlut_t const *lut,
                                    int r,
                                    int g,
                                    int b,
                                    int *best_idx,
                                    int *second_idx,
                                    uint64_t *best_dist,
                                    uint64_t *second_dist);
static int sixel_certlut_is_cell_safe(sixel_certlut_t const *lut,
                                  int best_idx,
                                  int second_idx,
                                  int size,
                                  uint64_t best_dist,
                                  uint64_t second_dist);
static uint32_t sixel_certlut_pool_alloc(sixel_certlut_t *lut, int *status);
static void sixel_certlut_assign_leaf(uint32_t *cell, int palette_index);
static void sixel_certlut_assign_branch(uint32_t *cell, uint32_t offset);
static uint8_t sixel_certlut_fallback(sixel_certlut_t const *lut,
                                  int r,
                                  int g,
                                  int b);
static int sixel_certlut_build_cell(sixel_certlut_t *lut,
                                uint32_t *cell,
                                int rmin,
                                int gmin,
                                int bmin,
                                int size);
static int
sixel_certlut_init(sixel_certlut_t *lut)
{
    int status;

    status = SIXEL_FALSE;
    if (lut == NULL) {
        goto end;
    }

    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    lut->wR = 1;
    lut->wG = 1;
    lut->wB = 1;
    lut->wR2 = 1U;
    lut->wG2 = 1U;
    lut->wB2 = 1U;
    memset(lut->wr_scale, 0, sizeof(lut->wr_scale));
    memset(lut->wg_scale, 0, sizeof(lut->wg_scale));
    memset(lut->wb_scale, 0, sizeof(lut->wb_scale));
    lut->wr_palette = NULL;
    lut->wg_palette = NULL;
    lut->wb_palette = NULL;
    lut->palette = NULL;
    lut->ncolors = 0;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    status = SIXEL_OK;

end:
    return status;
}

static void
sixel_certlut_release(sixel_certlut_t *lut)
{
    if (lut == NULL) {
        return;
    }
    free(lut->level0);
    free(lut->pool);
    free(lut->wr_palette);
    free(lut->wg_palette);
    free(lut->wb_palette);
    free(lut->kdnodes);
    lut->level0 = NULL;
    lut->pool = NULL;
    lut->pool_size = 0U;
    lut->pool_capacity = 0U;
    lut->wr_palette = NULL;
    lut->wg_palette = NULL;
    lut->wb_palette = NULL;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
}

static int
sixel_certlut_prepare_palette_terms(sixel_certlut_t *lut)
{
    int status;
    size_t count;
    int index;
    int32_t *wr_terms;
    int32_t *wg_terms;
    int32_t *wb_terms;

    status = SIXEL_FALSE;
    wr_terms = NULL;
    wg_terms = NULL;
    wb_terms = NULL;
    if (lut == NULL) {
        goto end;
    }
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    count = (size_t)lut->ncolors;
    wr_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wr_terms == NULL) {
        goto end;
    }
    wg_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wg_terms == NULL) {
        goto end;
    }
    wb_terms = (int32_t *)malloc(count * sizeof(int32_t));
    if (wb_terms == NULL) {
        goto end;
    }
    for (index = 0; index < lut->ncolors; ++index) {
        wr_terms[index] = lut->wR * (int)lut->palette[index].r;
        wg_terms[index] = lut->wG * (int)lut->palette[index].g;
        wb_terms[index] = lut->wB * (int)lut->palette[index].b;
    }
    free(lut->wr_palette);
    free(lut->wg_palette);
    free(lut->wb_palette);
    lut->wr_palette = wr_terms;
    lut->wg_palette = wg_terms;
    lut->wb_palette = wb_terms;
    wr_terms = NULL;
    wg_terms = NULL;
    wb_terms = NULL;
    status = SIXEL_OK;

end:
    free(wr_terms);
    free(wg_terms);
    free(wb_terms);
    return status;
}

static void
sixel_certlut_cell_center(int rmin, int gmin, int bmin, int size,
                        int *cr, int *cg, int *cb)
{
    int half;

    half = size / 2;
    *cr = rmin + half;
    *cg = gmin + half;
    *cb = bmin + half;
    if (size == 1) {
        *cr = rmin;
        *cg = gmin;
        *cb = bmin;
    }
}

static void
sixel_certlut_weight_init(sixel_certlut_t *lut, int wR, int wG, int wB)
{
    int i;

    lut->wR = wR;
    lut->wG = wG;
    lut->wB = wB;
    lut->wR2 = (uint64_t)wR * (uint64_t)wR;
    lut->wG2 = (uint64_t)wG * (uint64_t)wG;
    lut->wB2 = (uint64_t)wB * (uint64_t)wB;
    for (i = 0; i < 256; ++i) {
        lut->wr_scale[i] = wR * i;
        lut->wg_scale[i] = wG * i;
        lut->wb_scale[i] = wB * i;
    }
}

static uint64_t
sixel_certlut_distance_precomputed(sixel_certlut_t const *lut,
                                   int index,
                                   int32_t wr_r,
                                   int32_t wg_g,
                                   int32_t wb_b)
{
    uint64_t distance;
    int64_t diff;

    diff = (int64_t)wr_r - (int64_t)lut->wr_palette[index];
    distance = (uint64_t)(diff * diff);
    diff = (int64_t)wg_g - (int64_t)lut->wg_palette[index];
    distance += (uint64_t)(diff * diff);
    diff = (int64_t)wb_b - (int64_t)lut->wb_palette[index];
    distance += (uint64_t)(diff * diff);

    return distance;
}

static void
sixel_certlut_distance_pair(sixel_certlut_t const *lut, int r, int g, int b,
                          int *best_idx, int *second_idx,
                          uint64_t *best_dist, uint64_t *second_dist)
{
    int i;
    int best_candidate;
    int second_candidate;
    uint64_t best_value;
    uint64_t second_value;
    uint64_t distance;
    int rr;
    int gg;
    int bb;
    int32_t wr_r;
    int32_t wg_g;
    int32_t wb_b;

    best_candidate = (-1);
    second_candidate = (-1);
    best_value = UINT64_MAX;
    second_value = UINT64_MAX;
    rr = r;
    gg = g;
    bb = b;
    if (rr < 0) {
        rr = 0;
    } else if (rr > 255) {
        rr = 255;
    }
    if (gg < 0) {
        gg = 0;
    } else if (gg > 255) {
        gg = 255;
    }
    if (bb < 0) {
        bb = 0;
    } else if (bb > 255) {
        bb = 255;
    }
    wr_r = lut->wr_scale[rr];
    wg_g = lut->wg_scale[gg];
    wb_b = lut->wb_scale[bb];
    if (lut->kdnodes != NULL && lut->kdtree_root >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    lut->kdtree_root,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    &best_candidate,
                                    &best_value,
                                    &second_candidate,
                                    &second_value);
    } else {
        for (i = 0; i < lut->ncolors; ++i) {
            distance = sixel_certlut_distance_precomputed(lut,
                                                          i,
                                                          wr_r,
                                                          wg_g,
                                                          wb_b);
            if (distance < best_value) {
                second_value = best_value;
                second_candidate = best_candidate;
                best_value = distance;
                best_candidate = i;
            } else if (distance < second_value) {
                second_value = distance;
                second_candidate = i;
            }
        }
    }
    if (second_candidate < 0) {
        second_candidate = best_candidate;
        second_value = best_value;
    }
    *best_idx = best_candidate;
    *second_idx = second_candidate;
    *best_dist = best_value;
    *second_dist = second_value;
}

static int
sixel_certlut_is_cell_safe(sixel_certlut_t const *lut, int best_idx,
                         int second_idx, int size, uint64_t best_dist,
                         uint64_t second_dist)
{
    uint64_t delta_sq;
    uint64_t rhs;
    uint64_t weight_term;
    int64_t wr_delta;
    int64_t wg_delta;
    int64_t wb_delta;

    if (best_idx < 0 || second_idx < 0) {
        return 1;
    }

    /*
     * The certification bound compares the squared distance gap against the
     * palette separation scaled by the cube diameter.  If the gap wins the
     * entire cube maps to the current best palette entry.
     */
    delta_sq = second_dist - best_dist;
    wr_delta = (int64_t)lut->wr_palette[second_idx]
        - (int64_t)lut->wr_palette[best_idx];
    wg_delta = (int64_t)lut->wg_palette[second_idx]
        - (int64_t)lut->wg_palette[best_idx];
    wb_delta = (int64_t)lut->wb_palette[second_idx]
        - (int64_t)lut->wb_palette[best_idx];
    weight_term = (uint64_t)(wr_delta * wr_delta);
    weight_term += (uint64_t)(wg_delta * wg_delta);
    weight_term += (uint64_t)(wb_delta * wb_delta);
    rhs = (uint64_t)3 * (uint64_t)size * (uint64_t)size * weight_term;

    return delta_sq * delta_sq > rhs;
}

static uint32_t
sixel_certlut_pool_alloc(sixel_certlut_t *lut, int *status)
{
    uint32_t required;
    uint32_t next_capacity;
    uint32_t offset;
    uint8_t *resized;

    offset = 0U;
    if (status != NULL) {
        *status = SIXEL_FALSE;
    }
    required = lut->pool_size + (uint32_t)(8 * sizeof(uint32_t));
    if (required > lut->pool_capacity) {
        next_capacity = lut->pool_capacity;
        if (next_capacity == 0U) {
            next_capacity = (uint32_t)(8 * sizeof(uint32_t));
        }
        while (next_capacity < required) {
            if (next_capacity > UINT32_MAX / 2U) {
                return 0U;
            }
            next_capacity *= 2U;
        }
        resized = (uint8_t *)realloc(lut->pool, next_capacity);
        if (resized == NULL) {
            return 0U;
        }
        lut->pool = resized;
        lut->pool_capacity = next_capacity;
    }
    offset = lut->pool_size;
    memset(lut->pool + offset, 0, 8 * sizeof(uint32_t));
    lut->pool_size = required;
    if (status != NULL) {
        *status = SIXEL_OK;
    }

    return offset;
}

static void
sixel_certlut_assign_leaf(uint32_t *cell, int palette_index)
{
    *cell = 0x80000000U | (uint32_t)(palette_index & 0xff);
}

static void
sixel_certlut_assign_branch(uint32_t *cell, uint32_t offset)
{
    *cell = SIXEL_LUT_BRANCH_FLAG | (offset & 0x3fffffffU);
}

static int
sixel_certlut_palette_component(sixel_certlut_t const *lut,
                                int index, int axis)
{
    sixel_certlut_color_t const *color;

    color = &lut->palette[index];
    if (axis == 0) {
        return (int)color->r;
    }
    if (axis == 1) {
        return (int)color->g;
    }
    return (int)color->b;
}

static void
sixel_certlut_sort_indices(sixel_certlut_t const *lut,
                           int *indices, int count, int axis)
{
    int i;
    int j;
    int key;
    int key_value;
    int current_value;

    for (i = 1; i < count; ++i) {
        key = indices[i];
        key_value = sixel_certlut_palette_component(lut, key, axis);
        j = i - 1;
        while (j >= 0) {
            current_value = sixel_certlut_palette_component(lut,
                                                            indices[j],
                                                            axis);
            if (current_value <= key_value) {
                break;
            }
            indices[j + 1] = indices[j];
            --j;
        }
        indices[j + 1] = key;
    }
}

static int
sixel_certlut_kdtree_build_recursive(sixel_certlut_t *lut,
                                     int *indices,
                                     int count,
                                     int depth)
{
    int axis;
    int median;
    int node_index;

    if (count <= 0) {
        return -1;
    }

    axis = depth % 3;
    sixel_certlut_sort_indices(lut, indices, count, axis);
    median = count / 2;
    node_index = lut->kdnodes_count;
    if (node_index >= lut->ncolors) {
        return -1;
    }
    lut->kdnodes_count++;
    lut->kdnodes[node_index].index = indices[median];
    lut->kdnodes[node_index].axis = (unsigned char)axis;
    lut->kdnodes[node_index].left =
        sixel_certlut_kdtree_build_recursive(lut,
                                             indices,
                                             median,
                                             depth + 1);
    lut->kdnodes[node_index].right =
        sixel_certlut_kdtree_build_recursive(lut,
                                             indices + median + 1,
                                             count - median - 1,
                                             depth + 1);

    return node_index;
}

static SIXELSTATUS
sixel_certlut_kdtree_build(sixel_certlut_t *lut)
{
    SIXELSTATUS status;
    int *indices;
    int i;

    status = SIXEL_FALSE;
    indices = NULL;
    lut->kdnodes = NULL;
    lut->kdnodes_count = 0;
    lut->kdtree_root = -1;
    if (lut->ncolors <= 0) {
        status = SIXEL_OK;
        goto end;
    }
    lut->kdnodes = (sixel_certlut_node_t *)
        calloc((size_t)lut->ncolors, sizeof(sixel_certlut_node_t));
    if (lut->kdnodes == NULL) {
        goto end;
    }
    indices = (int *)malloc((size_t)lut->ncolors * sizeof(int));
    if (indices == NULL) {
        goto end;
    }
    for (i = 0; i < lut->ncolors; ++i) {
        indices[i] = i;
    }
    lut->kdnodes_count = 0;
    lut->kdtree_root = sixel_certlut_kdtree_build_recursive(lut,
                                                            indices,
                                                            lut->ncolors,
                                                            0);
    if (lut->kdtree_root < 0) {
        goto end;
    }
    status = SIXEL_OK;

end:
    free(indices);
    if (SIXEL_FAILED(status)) {
        free(lut->kdnodes);
        lut->kdnodes = NULL;
        lut->kdnodes_count = 0;
        lut->kdtree_root = -1;
    }

    return status;
}

static uint64_t
sixel_certlut_axis_distance(sixel_certlut_t const *lut, int diff, int axis)
{
    uint64_t weight;
    uint64_t abs_diff;

    abs_diff = (uint64_t)(diff < 0 ? -diff : diff);
    if (axis == 0) {
        weight = lut->wR2;
    } else if (axis == 1) {
        weight = lut->wG2;
    } else {
        weight = lut->wB2;
    }

    return weight * abs_diff * abs_diff;
}

static void
sixel_certlut_consider_candidate(sixel_certlut_t const *lut,
                                 int candidate,
                                 int32_t wr_r,
                                 int32_t wg_g,
                                 int32_t wb_b,
                                 int *best_idx,
                                 uint64_t *best_dist,
                                 int *second_idx,
                                 uint64_t *second_dist)
{
    uint64_t distance;

    distance = sixel_certlut_distance_precomputed(lut,
                                                  candidate,
                                                  wr_r,
                                                  wg_g,
                                                  wb_b);
    if (distance < *best_dist) {
        *second_dist = *best_dist;
        *second_idx = *best_idx;
        *best_dist = distance;
        *best_idx = candidate;
    } else if (distance < *second_dist) {
        *second_dist = distance;
        *second_idx = candidate;
    }
}

static void
sixel_certlut_kdtree_search(sixel_certlut_t const *lut,
                            int node_index,
                            int r,
                            int g,
                            int b,
                            int32_t wr_r,
                            int32_t wg_g,
                            int32_t wb_b,
                            int *best_idx,
                            uint64_t *best_dist,
                            int *second_idx,
                            uint64_t *second_dist)
{
    sixel_certlut_node_t const *node;
    int axis;
    int value;
    int diff;
    int near_child;
    int far_child;
    uint64_t axis_bound;
    int component;

    if (node_index < 0) {
        return;
    }
    node = &lut->kdnodes[node_index];
    sixel_certlut_consider_candidate(lut,
                                     node->index,
                                     wr_r,
                                     wg_g,
                                     wb_b,
                                     best_idx,
                                     best_dist,
                                     second_idx,
                                     second_dist);

    axis = (int)node->axis;
    value = sixel_certlut_palette_component(lut, node->index, axis);
    if (axis == 0) {
        component = r;
    } else if (axis == 1) {
        component = g;
    } else {
        component = b;
    }
    diff = component - value;
    if (diff <= 0) {
        near_child = node->left;
        far_child = node->right;
    } else {
        near_child = node->right;
        far_child = node->left;
    }
    if (near_child >= 0) {
        sixel_certlut_kdtree_search(lut,
                                    near_child,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
    axis_bound = sixel_certlut_axis_distance(lut, diff, axis);
    if (far_child >= 0 && axis_bound <= *second_dist) {
        sixel_certlut_kdtree_search(lut,
                                    far_child,
                                    r,
                                    g,
                                    b,
                                    wr_r,
                                    wg_g,
                                    wb_b,
                                    best_idx,
                                    best_dist,
                                    second_idx,
                                    second_dist);
    }
}

static uint8_t
sixel_certlut_fallback(sixel_certlut_t const *lut, int r, int g, int b)
{
    int best_idx;
    int second_idx;
    uint64_t best_dist;
    uint64_t second_dist;

    best_idx = -1;
    second_idx = -1;
    best_dist = 0U;
    second_dist = 0U;
    if (lut == NULL) {
        return 0U;
    }
    /*
     * The lazy builder may fail when allocations run out.  Fall back to a
     * direct brute-force palette search so lookups still succeed even in low
     * memory conditions.
     */
    sixel_certlut_distance_pair(lut,
                              r,
                              g,
                              b,
                              &best_idx,
                              &second_idx,
                              &best_dist,
                              &second_dist);
    if (best_idx < 0) {
        return 0U;
    }

    return (uint8_t)best_idx;
}

SIXELSTATUS
sixel_certlut_build_cell(sixel_certlut_t *lut, uint32_t *cell,
                         int rmin, int gmin, int bmin, int size)
{
    SIXELSTATUS status;
    int cr;
    int cg;
    int cb;
    int best_idx;
    int second_idx;
    uint64_t best_dist;
    uint64_t second_dist;
    uint32_t offset;
    int branch_status;
    uint8_t *pool_before;
    size_t pool_size_before;
    uint32_t cell_offset;
    int cell_in_pool;

    if (cell == NULL || lut == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }
    if (*cell == 0U) {
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "build_cell rmin=%d gmin=%d bmin=%d size=%d\n",
                rmin,
                gmin,
                bmin,
                size);
#endif
    }
    if (*cell != 0U) {
        status = SIXEL_OK;
        goto end;
    }

    /*
     * Each node represents an axis-aligned cube in RGB space.  The builder
     * certifies the dominant palette index by checking the distance gap at
     * the cell center.  When certification fails the cube is split into eight
     * octants backed by a pool block.  Children remain unbuilt until lookups
     * descend into them, keeping the workload proportional to actual queries.
     */
    status = SIXEL_FALSE;
    sixel_certlut_cell_center(rmin, gmin, bmin, size, &cr, &cg, &cb);
    sixel_certlut_distance_pair(lut, cr, cg, cb, &best_idx, &second_idx,
                              &best_dist, &second_dist);
    if (best_idx < 0) {
        best_idx = 0;
    }
    if (size == 1) {
        sixel_certlut_assign_leaf(cell, best_idx);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "  leaf idx=%d\n",
                best_idx);
#endif
        status = SIXEL_OK;
        goto end;
    }
    if (sixel_certlut_is_cell_safe(lut, best_idx, second_idx, size,
                                 best_dist, second_dist)) {
        sixel_certlut_assign_leaf(cell, best_idx);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "  safe leaf idx=%d\n",
                best_idx);
#endif
        status = SIXEL_OK;
        goto end;
    }
    pool_before = lut->pool;
    pool_size_before = lut->pool_size;
    cell_in_pool = 0;
    cell_offset = 0U;
    /*
     * The pool may grow while building descendants.  Remember the caller's
     * offset so the cell pointer can be refreshed after realloc moves the
     * backing storage.
     */
    if (pool_before != NULL) {
        if ((uint8_t *)(void *)cell >= pool_before
                && (size_t)((uint8_t *)(void *)cell - pool_before)
                        < pool_size_before) {
            cell_in_pool = 1;
            cell_offset = (uint32_t)((uint8_t *)(void *)cell - pool_before);
        }
    }
    offset = sixel_certlut_pool_alloc(lut, &branch_status);
    if (branch_status != SIXEL_OK) {
        goto end;
    }
    if (cell_in_pool != 0) {
        cell = (uint32_t *)(void *)(lut->pool + cell_offset);
    }
    sixel_certlut_assign_branch(cell, offset);
#ifdef DEBUG_LUT_TRACE
    fprintf(stderr,
            "  branch offset=%u\n",
            offset);
#endif
    status = SIXEL_OK;

end:
    return status;
}

SIXELSTATUS
sixel_certlut_build(sixel_certlut_t *lut, sixel_certlut_color_t const *palette,
                    int ncolors, int wR, int wG, int wB)
{
    SIXELSTATUS status;
    int initialized;
    size_t level0_count;
    status = SIXEL_FALSE;
    initialized = sixel_certlut_init(lut);
    if (SIXEL_FAILED(initialized)) {
        goto end;
    }
    lut->palette = palette;
    lut->ncolors = ncolors;
    sixel_certlut_weight_init(lut, wR, wG, wB);
    status = sixel_certlut_prepare_palette_terms(lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_certlut_kdtree_build(lut);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    level0_count = (size_t)64 * (size_t)64 * (size_t)64;
    lut->level0 = (uint32_t *)calloc(level0_count, sizeof(uint32_t));
    if (lut->level0 == NULL) {
        goto end;
    }
    /*
     * Level 0 cells start uninitialized.  The lookup routine materializes
     * individual subtrees on demand so we avoid evaluating the entire
     * 64x64x64 grid upfront.
     */
    status = SIXEL_OK;

end:
    if (SIXEL_FAILED(status)) {
        sixel_certlut_release(lut);
    }
    return status;
}

uint8_t
sixel_certlut_lookup(sixel_certlut_t *lut, uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t entry;
    uint32_t offset;
    uint32_t index;
    uint32_t *children;
    uint32_t *cell;
    int shift;
    int child;
    int status;
    int size;
    int rmin;
    int gmin;
    int bmin;
    int step;
    if (lut == NULL || lut->level0 == NULL) {
        return 0U;
    }
    /*
     * Cells are created lazily.  A zero entry indicates an uninitialized
     * subtree, so the builder is invoked with the cube bounds of the current
     * traversal.  Should allocation fail we fall back to a direct brute-force
     * palette search for the queried pixel.
     */
    index = ((uint32_t)(r >> 2) << 12)
          | ((uint32_t)(g >> 2) << 6)
          | (uint32_t)(b >> 2);
    cell = lut->level0 + index;
    size = 4;
    rmin = (int)(r & 0xfc);
    gmin = (int)(g & 0xfc);
    bmin = (int)(b & 0xfc);
    entry = *cell;
    if (entry == 0U) {
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "lookup build level0 r=%u g=%u b=%u\n",
                (unsigned int)r,
                (unsigned int)g,
                (unsigned int)b);
#endif
        status = sixel_certlut_build_cell(lut, cell, rmin, gmin, bmin, size);
        if (SIXEL_FAILED(status)) {
            return sixel_certlut_fallback(lut,
                                          (int)r,
                                          (int)g,
                                          (int)b);
        }
        entry = *cell;
    }
    shift = 1;
    while ((entry & 0x80000000U) == 0U) {
        offset = entry & 0x3fffffffU;
        children = (uint32_t *)(void *)(lut->pool + offset);
        child = (((int)(r >> shift) & 1) << 2)
              | (((int)(g >> shift) & 1) << 1)
              | ((int)(b >> shift) & 1);
#ifdef DEBUG_LUT_TRACE
        fprintf(stderr,
                "descend child=%d size=%d offset=%u\n",
                child,
                size,
                offset);
#endif
        step = size / 2;
        if (step <= 0) {
            step = 1;
        }
        rmin += step * ((child >> 2) & 1);
        gmin += step * ((child >> 1) & 1);
        bmin += step * (child & 1);
        size = step;
        cell = children + (size_t)child;
        entry = *cell;
        if (entry == 0U) {
#ifdef DEBUG_LUT_TRACE
            fprintf(stderr,
                    "lookup build child size=%d rmin=%d gmin=%d bmin=%d\n",
                    size,
                    rmin,
                    gmin,
                    bmin);
#endif
            status = sixel_certlut_build_cell(lut,
                                              cell,
                                              rmin,
                                              gmin,
                                              bmin,
                                              size);
            if (SIXEL_FAILED(status)) {
                return sixel_certlut_fallback(lut,
                                              (int)r,
                                              (int)g,
                                              (int)b);
            }
            children = (uint32_t *)(void *)(lut->pool + offset);
            cell = children + (size_t)child;
            entry = *cell;
        }
        if (size == 1) {
            break;
        }
        if (shift == 0) {
            break;
        }
        --shift;
    }

    return (uint8_t)(entry & 0xffU);
}

void
sixel_certlut_free(sixel_certlut_t *lut)
{
    sixel_certlut_release(lut);
    if (lut != NULL) {
        lut->palette = NULL;
        lut->ncolors = 0;
    }
}

SIXELSTATUS
sixel_lut_new(sixel_lut_t **out,
              int policy,
              sixel_allocator_t *allocator)
{
    sixel_lut_t *lut;
    SIXELSTATUS status;

    if (out == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut = (sixel_lut_t *)malloc(sizeof(sixel_lut_t));
    if (lut == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }
    memset(lut, 0, sizeof(sixel_lut_t));
    lut->allocator = allocator;
    lut->policy = sixel_lut_policy_normalize(policy);
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
    lut->palette = NULL;
    lut->cache = NULL;
    lut->cache_slots = 0U;
    lut->cache_ready = 0;
    lut->cert_ready = 0;
    status = sixel_certlut_init(&lut->cert);
    if (SIXEL_FAILED(status)) {
        free(lut);
        sixel_helper_set_additional_message(
            "sixel_lut_new: unable to initialize certlut state.");
        return status;
    }

    *out = lut;

    return SIXEL_OK;
}

SIXELSTATUS
sixel_lut_configure(sixel_lut_t *lut,
                    unsigned char const *palette,
                    int depth,
                    int ncolors,
                    int complexion,
                    int wR,
                    int wG,
                    int wB,
                    int policy)
{
    SIXELSTATUS status;
    int normalized;

    if (lut == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (palette == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (depth <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    lut->palette = palette;
    lut->depth = depth;
    lut->ncolors = ncolors;
    lut->complexion = complexion;
    normalized = sixel_lut_policy_normalize(policy);
    lut->policy = normalized;
    lut->control = histogram_control_make_for_policy((unsigned int)depth,
                                                     normalized);
    lut->cache_ready = 0;
    lut->cert_ready = 0;

    if (sixel_lut_policy_uses_cache(normalized)) {
        if (depth != 3) {
            sixel_helper_set_additional_message(
                "sixel_lut_configure: fast LUT requires RGB pixels.");
            return SIXEL_BAD_ARGUMENT;
        }
        status = sixel_lut_prepare_cache(lut);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    } else {
        sixel_lut_release_cache(lut);
        status = SIXEL_OK;
    }

    if (normalized == SIXEL_LUT_POLICY_CERTLUT) {
        status = sixel_certlut_build(&lut->cert,
                                     (sixel_certlut_color_t const *)palette,
                                     ncolors,
                                     wR,
                                     wG,
                                     wB);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        lut->cert_ready = 1;
    }

    return SIXEL_OK;
}

int
sixel_lut_map_pixel(sixel_lut_t *lut, unsigned char const *pixel)
{
    if (lut == NULL || pixel == NULL) {
        return 0;
    }
    if (lut->policy == SIXEL_LUT_POLICY_CERTLUT) {
        if (!lut->cert_ready) {
            return 0;
        }
        return (int)sixel_certlut_lookup(&lut->cert,
                                         pixel[0],
                                         pixel[1],
                                         pixel[2]);
    }

    return sixel_lut_lookup_fast(lut, pixel);
}

uint8_t
sixel_lut_map_rgb(sixel_lut_t *lut, uint8_t r, uint8_t g, uint8_t b)
{
    unsigned char pixel[3];

    if (lut == NULL) {
        return 0U;
    }
    pixel[0] = r;
    pixel[1] = g;
    pixel[2] = b;
    return (uint8_t)sixel_lut_map_pixel(lut, pixel);
}

void
sixel_lut_clear(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lut_release_cache(lut);
    if (lut->cert_ready) {
        sixel_certlut_free(&lut->cert);
        lut->cert_ready = 0;
    }
    lut->palette = NULL;
    lut->depth = 0;
    lut->ncolors = 0;
    lut->complexion = 1;
}

void
sixel_lut_unref(sixel_lut_t *lut)
{
    if (lut == NULL) {
        return;
    }

    sixel_lut_clear(lut);
    free(lut);
}
