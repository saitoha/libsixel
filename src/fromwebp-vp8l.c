/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See AUTHORS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#if defined(HAVE_CONFIG_H)
# include "config.h"
#endif

/* STDC_HEADERS */
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDINT_H
# include <stdint.h>
#endif

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t)-1)
#endif

#include <sixel.h>

#include "allocator.h"
#include "compat_stub.h"
#include "fromwebp-internal.h"

#define SIXEL_WEBP_LITERAL_CODES 256
#define SIXEL_WEBP_LENGTH_CODES  24
#define SIXEL_WEBP_DISTANCE_CODES 40
#define SIXEL_WEBP_CODELEN_CODES 19
#define SIXEL_WEBP_MAX_HUFFMAN_BITS 15

#define SIXEL_WEBP_TRANSFORM_MAX 4
typedef struct sixel_webp_bit_reader {
    unsigned char const *data;
    size_t size;
    size_t position;
    uint64_t bit_buffer;
    int bit_count;
} sixel_webp_bit_reader_t;

typedef struct sixel_webp_huffman_node {
    int left;
    int right;
    int symbol;
} sixel_webp_huffman_node_t;

typedef struct sixel_webp_huffman {
    int is_single;
    int single_symbol;
    sixel_webp_huffman_node_t *nodes;
    int node_count;
    int node_capacity;
} sixel_webp_huffman_t;

typedef struct sixel_webp_prefix_group {
    sixel_webp_huffman_t code[5];
} sixel_webp_prefix_group_t;

typedef struct sixel_webp_transform {
    int type;
    int size_bits;
    int width_bits;
    int width_before;
    int data_width;
    int data_height;
    int color_table_size;
    uint32_t *data;
} sixel_webp_transform_t;

static int const sixel_webp_code_length_order[SIXEL_WEBP_CODELEN_CODES] = {
    17, 18, 0, 1, 2, 3, 4, 5, 16, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

static int const sixel_webp_distance_map[120][2] = {
    { 0, 1 }, { 1, 0 }, { 1, 1 }, { -1, 1 }, { 0, 2 }, { 2, 0 },
    { 1, 2 }, { -1, 2 }, { 2, 1 }, { -2, 1 }, { 2, 2 }, { -2, 2 },
    { 0, 3 }, { 3, 0 }, { 1, 3 }, { -1, 3 }, { 3, 1 }, { -3, 1 },
    { 2, 3 }, { -2, 3 }, { 3, 2 }, { -3, 2 }, { 0, 4 }, { 4, 0 },
    { 1, 4 }, { -1, 4 }, { 4, 1 }, { -4, 1 }, { 3, 3 }, { -3, 3 },
    { 2, 4 }, { -2, 4 }, { 4, 2 }, { -4, 2 }, { 0, 5 }, { 3, 4 },
    { -3, 4 }, { 4, 3 }, { -4, 3 }, { 5, 0 }, { 1, 5 }, { -1, 5 },
    { 5, 1 }, { -5, 1 }, { 2, 5 }, { -2, 5 }, { 5, 2 }, { -5, 2 },
    { 4, 4 }, { -4, 4 }, { 3, 5 }, { -3, 5 }, { 5, 3 }, { -5, 3 },
    { 0, 6 }, { 6, 0 }, { 1, 6 }, { -1, 6 }, { 6, 1 }, { -6, 1 },
    { 2, 6 }, { -2, 6 }, { 6, 2 }, { -6, 2 }, { 4, 5 }, { -4, 5 },
    { 5, 4 }, { -5, 4 }, { 3, 6 }, { -3, 6 }, { 6, 3 }, { -6, 3 },
    { 0, 7 }, { 7, 0 }, { 1, 7 }, { -1, 7 }, { 5, 5 }, { -5, 5 },
    { 7, 1 }, { -7, 1 }, { 4, 6 }, { -4, 6 }, { 6, 4 }, { -6, 4 },
    { 2, 7 }, { -2, 7 }, { 7, 2 }, { -7, 2 }, { 3, 7 }, { -3, 7 },
    { 7, 3 }, { -7, 3 }, { 5, 6 }, { -5, 6 }, { 6, 5 }, { -6, 5 },
    { 8, 0 }, { 4, 7 }, { -4, 7 }, { 7, 4 }, { -7, 4 }, { 8, 1 },
    { 8, 2 }, { 6, 6 }, { -6, 6 }, { 8, 3 }, { 5, 7 }, { -5, 7 },
    { 7, 5 }, { -7, 5 }, { 8, 4 }, { 6, 7 }, { -6, 7 }, { 7, 6 },
    { -7, 6 }, { 8, 5 }, { 7, 7 }, { -7, 7 }, { 8, 6 }, { 8, 7 }
};
static uint32_t
sixel_webp_make_argb(unsigned int a,
                     unsigned int r,
                     unsigned int g,
                     unsigned int b)
{
    return ((uint32_t)(a & 0xffu) << 24)
        | ((uint32_t)(r & 0xffu) << 16)
        | ((uint32_t)(g & 0xffu) << 8)
        | (uint32_t)(b & 0xffu);
}

static unsigned int
sixel_webp_argb_a(uint32_t argb)
{
    return (unsigned int)((argb >> 24) & 0xffu);
}

static unsigned int
sixel_webp_argb_r(uint32_t argb)
{
    return (unsigned int)((argb >> 16) & 0xffu);
}

static unsigned int
sixel_webp_argb_g(uint32_t argb)
{
    return (unsigned int)((argb >> 8) & 0xffu);
}

static unsigned int
sixel_webp_argb_b(uint32_t argb)
{
    return (unsigned int)(argb & 0xffu);
}

static int
sixel_webp_u8_to_i8(unsigned int value)
{
    if (value < 128u) {
        return (int)value;
    }
    return (int)value - 256;
}

static int
sixel_webp_clamp_byte(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

static int
sixel_webp_mul_shift5(int t, int c)
{
    int product;

    product = t * c;
    if (product >= 0) {
        return product >> 5;
    }
    return -(((-product) + 31) >> 5);
}

static int
sixel_webp_average2(int a, int b)
{
    return (a + b) >> 1;
}

static size_t
sixel_webp_div_round_up_size(size_t num, size_t den)
{
    if (den == 0u) {
        return 0u;
    }
    if (num == 0u) {
        return 0u;
    }
    if (num > SIZE_MAX - (den - 1u)) {
        return 0u;
    }
    return (num + den - 1u) / den;
}

static SIXELSTATUS
sixel_webp_validate_dimensions(int width, int height)
{
    size_t pixel_count;

    pixel_count = 0u;
    if (width <= 0 || height <= 0) {
        sixel_helper_set_additional_message(
            "builtin webp: invalid image dimensions.");
        return SIXEL_BAD_INPUT;
    }
    if (width > SIXEL_WEBP_MAX_DIMENSION ||
        height > SIXEL_WEBP_MAX_DIMENSION) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    if ((size_t)width > SIZE_MAX / (size_t)height) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIXEL_WEBP_MAX_IMAGE_PIXELS) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }
    return SIXEL_OK;
}

static void
sixel_webp_bit_reader_init(sixel_webp_bit_reader_t *br,
                           unsigned char const *data,
                           size_t size)
{
    if (br == NULL) {
        return;
    }
    br->data = data;
    br->size = size;
    br->position = 0u;
    br->bit_buffer = 0u;
    br->bit_count = 0;
}

static int
sixel_webp_bit_reader_read_bits(sixel_webp_bit_reader_t *br,
                                int nbits,
                                uint32_t *value)
{
    uint64_t mask;

    mask = 0u;
    if (br == NULL || value == NULL || nbits < 0 || nbits > 24) {
        return 0;
    }

    while (br->bit_count < nbits) {
        if (br->position >= br->size) {
            return 0;
        }
        br->bit_buffer |= ((uint64_t)br->data[br->position])
            << br->bit_count;
        br->bit_count += 8;
        br->position++;
    }

    if (nbits == 0) {
        *value = 0u;
        return 1;
    }

    mask = ((uint64_t)1u << nbits) - 1u;
    *value = (uint32_t)(br->bit_buffer & mask);
    br->bit_buffer >>= nbits;
    br->bit_count -= nbits;
    return 1;
}

static void
sixel_webp_huffman_reset(sixel_webp_huffman_t *huff,
                         sixel_allocator_t *allocator)
{
    if (huff == NULL) {
        return;
    }
    if (huff->nodes != NULL && allocator != NULL) {
        sixel_allocator_free(allocator, huff->nodes);
    }
    huff->is_single = 0;
    huff->single_symbol = 0;
    huff->nodes = NULL;
    huff->node_count = 0;
    huff->node_capacity = 0;
}

static int
sixel_webp_huffman_add_node(sixel_webp_huffman_t *huff,
                            int *new_index,
                            sixel_allocator_t *allocator)
{
    if (huff == NULL || new_index == NULL || allocator == NULL) {
        return 0;
    }
    if (huff->node_count >= huff->node_capacity) {
        return 0;
    }
    *new_index = huff->node_count;
    huff->nodes[*new_index].left = -1;
    huff->nodes[*new_index].right = -1;
    huff->nodes[*new_index].symbol = -1;
    huff->node_count++;
    return 1;
}

static int
sixel_webp_huffman_build(sixel_webp_huffman_t *huff,
                         unsigned char const *lengths,
                         int alphabet_size,
                         sixel_allocator_t *allocator)
{
    int symbol;
    int bit_length;
    int nonzero_count;
    int single_symbol;
    int bl_count[SIXEL_WEBP_MAX_HUFFMAN_BITS + 1];
    unsigned int next_code[SIXEL_WEBP_MAX_HUFFMAN_BITS + 1];
    int left;
    unsigned int code;
    int node_index;
    int bit_index;
    int bit;
    int child;

    symbol = 0;
    bit_length = 0;
    nonzero_count = 0;
    single_symbol = 0;
    left = 0;
    code = 0u;
    node_index = 0;
    bit_index = 0;
    bit = 0;
    child = 0;

    if (huff == NULL || lengths == NULL || alphabet_size <= 0 ||
        allocator == NULL) {
        return 0;
    }

    sixel_webp_huffman_reset(huff, allocator);

    for (bit_length = 0; bit_length <= SIXEL_WEBP_MAX_HUFFMAN_BITS;
         ++bit_length) {
        bl_count[bit_length] = 0;
        next_code[bit_length] = 0u;
    }

    for (symbol = 0; symbol < alphabet_size; ++symbol) {
        bit_length = (int)lengths[symbol];
        if (bit_length < 0 || bit_length > SIXEL_WEBP_MAX_HUFFMAN_BITS) {
            return 0;
        }
        if (bit_length != 0) {
            bl_count[bit_length]++;
            nonzero_count++;
            single_symbol = symbol;
        }
    }

    if (nonzero_count == 0) {
        huff->is_single = 1;
        huff->single_symbol = 0;
        return 1;
    }
    if (nonzero_count == 1) {
        huff->is_single = 1;
        huff->single_symbol = single_symbol;
        return 1;
    }

    left = 1;
    for (bit_length = 1; bit_length <= SIXEL_WEBP_MAX_HUFFMAN_BITS;
         ++bit_length) {
        left <<= 1;
        left -= bl_count[bit_length];
        if (left < 0) {
            return 0;
        }
    }
    if (left != 0) {
        return 0;
    }

    huff->node_capacity = alphabet_size * 2 + 1;
    huff->nodes = (sixel_webp_huffman_node_t *)sixel_allocator_calloc(
        allocator,
        (size_t)huff->node_capacity,
        sizeof(*huff->nodes));
    if (huff->nodes == NULL) {
        return 0;
    }

    if (!sixel_webp_huffman_add_node(huff, &node_index, allocator)) {
        sixel_webp_huffman_reset(huff, allocator);
        return 0;
    }

    code = 0u;
    for (bit_length = 1; bit_length <= SIXEL_WEBP_MAX_HUFFMAN_BITS;
         ++bit_length) {
        code = (code + (unsigned int)bl_count[bit_length - 1]) << 1;
        next_code[bit_length] = code;
    }

    for (symbol = 0; symbol < alphabet_size; ++symbol) {
        bit_length = (int)lengths[symbol];
        if (bit_length == 0) {
            continue;
        }

        code = next_code[bit_length]++;
        node_index = 0;
        for (bit_index = bit_length - 1; bit_index >= 0; --bit_index) {
            bit = (int)((code >> bit_index) & 1u);
            child = bit == 0 ? huff->nodes[node_index].left
                             : huff->nodes[node_index].right;
            if (child < 0) {
                if (!sixel_webp_huffman_add_node(huff, &child, allocator)) {
                    sixel_webp_huffman_reset(huff, allocator);
                    return 0;
                }
                if (bit == 0) {
                    huff->nodes[node_index].left = child;
                } else {
                    huff->nodes[node_index].right = child;
                }
            }
            node_index = child;
        }

        if (huff->nodes[node_index].symbol >= 0) {
            sixel_webp_huffman_reset(huff, allocator);
            return 0;
        }
        huff->nodes[node_index].symbol = symbol;
    }

    return 1;
}

static SIXELSTATUS
sixel_webp_huffman_read_symbol(sixel_webp_huffman_t const *huff,
                               sixel_webp_bit_reader_t *br,
                               int *symbol)
{
    uint32_t bit_value;
    int node;
    int depth;

    bit_value = 0u;
    node = 0;
    depth = 0;

    if (huff == NULL || br == NULL || symbol == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (huff->is_single != 0) {
        *symbol = huff->single_symbol;
        return SIXEL_OK;
    }
    if (huff->nodes == NULL || huff->node_count <= 0) {
        sixel_helper_set_additional_message(
            "builtin webp: invalid Huffman table.");
        return SIXEL_BAD_INPUT;
    }

    node = 0;
    for (depth = 0; depth <= SIXEL_WEBP_MAX_HUFFMAN_BITS + 1; ++depth) {
        if (huff->nodes[node].symbol >= 0) {
            *symbol = huff->nodes[node].symbol;
            return SIXEL_OK;
        }
        if (!sixel_webp_bit_reader_read_bits(br, 1, &bit_value)) {
            sixel_helper_set_additional_message(
                "builtin webp: truncated Huffman-coded data.");
            return SIXEL_BAD_INPUT;
        }
        node = bit_value == 0u ? huff->nodes[node].left
                               : huff->nodes[node].right;
        if (node < 0 || node >= huff->node_count) {
            sixel_helper_set_additional_message(
                "builtin webp: invalid Huffman symbol.");
            return SIXEL_BAD_INPUT;
        }
    }

    sixel_helper_set_additional_message(
        "builtin webp: Huffman code length exceeds limit.");
    return SIXEL_BAD_INPUT;
}

static SIXELSTATUS
sixel_webp_read_normal_code_lengths(sixel_webp_bit_reader_t *br,
                                    unsigned char *lengths,
                                    int alphabet_size,
                                    sixel_allocator_t *allocator)
{
    unsigned char code_length_lengths[SIXEL_WEBP_CODELEN_CODES];
    sixel_webp_huffman_t code_length_tree;
    uint32_t bits;
    int num_code_lengths;
    int i;
    int use_reduced_alphabet;
    int length_nbits;
    int max_symbol;
    int symbols_remaining;
    int index;
    int symbol;
    int repeat;
    int repeat_value;
    SIXELSTATUS status;

    bits = 0u;
    num_code_lengths = 0;
    i = 0;
    use_reduced_alphabet = 0;
    length_nbits = 0;
    max_symbol = 0;
    symbols_remaining = 0;
    index = 0;
    symbol = 0;
    repeat = 0;
    repeat_value = 8;
    status = SIXEL_OK;
    memset(&code_length_tree, 0, sizeof(code_length_tree));

    if (br == NULL || lengths == NULL || alphabet_size <= 0 ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(code_length_lengths, 0, sizeof(code_length_lengths));

    if (!sixel_webp_bit_reader_read_bits(br, 4, &bits)) {
        sixel_helper_set_additional_message(
            "builtin webp: truncated code-length header.");
        return SIXEL_BAD_INPUT;
    }
    num_code_lengths = 4 + (int)bits;
    if (num_code_lengths < 4 ||
        num_code_lengths > SIXEL_WEBP_CODELEN_CODES) {
        sixel_helper_set_additional_message(
            "builtin webp: invalid code-length table size.");
        return SIXEL_BAD_INPUT;
    }

    for (i = 0; i < num_code_lengths; ++i) {
        if (!sixel_webp_bit_reader_read_bits(br, 3, &bits)) {
            sixel_helper_set_additional_message(
                "builtin webp: truncated code-length table.");
            return SIXEL_BAD_INPUT;
        }
        code_length_lengths[sixel_webp_code_length_order[i]]
            = (unsigned char)bits;
    }

    if (!sixel_webp_huffman_build(&code_length_tree,
                                  code_length_lengths,
                                  SIXEL_WEBP_CODELEN_CODES,
                                  allocator)) {
        sixel_helper_set_additional_message(
            "builtin webp: invalid code-length Huffman tree.");
        return SIXEL_BAD_INPUT;
    }

    if (!sixel_webp_bit_reader_read_bits(br, 1, &bits)) {
        sixel_webp_huffman_reset(&code_length_tree, allocator);
        sixel_helper_set_additional_message(
            "builtin webp: truncated alphabet-range flag.");
        return SIXEL_BAD_INPUT;
    }
    use_reduced_alphabet = (int)bits;

    if (use_reduced_alphabet == 0) {
        max_symbol = alphabet_size;
    } else {
        if (!sixel_webp_bit_reader_read_bits(br, 3, &bits)) {
            sixel_webp_huffman_reset(&code_length_tree, allocator);
            sixel_helper_set_additional_message(
                "builtin webp: truncated reduced alphabet size.");
            return SIXEL_BAD_INPUT;
        }
        length_nbits = 2 + 2 * (int)bits;
        if (!sixel_webp_bit_reader_read_bits(br, length_nbits, &bits)) {
            sixel_webp_huffman_reset(&code_length_tree, allocator);
            sixel_helper_set_additional_message(
                "builtin webp: truncated reduced alphabet value.");
            return SIXEL_BAD_INPUT;
        }
        max_symbol = 2 + (int)bits;
        if (max_symbol > alphabet_size) {
            sixel_webp_huffman_reset(&code_length_tree, allocator);
            sixel_helper_set_additional_message(
                "builtin webp: reduced alphabet exceeds symbol range.");
            return SIXEL_BAD_INPUT;
        }
    }

    memset(lengths, 0, (size_t)alphabet_size);
    index = 0;
    repeat_value = 8;
    /*
     * max_symbol limits how many code-length symbols are read from the
     * bitstream, not how many output entries those symbols expand to.
     */
    symbols_remaining = max_symbol;
    while (index < alphabet_size) {
        if (symbols_remaining <= 0) {
            break;
        }
        --symbols_remaining;
        status = sixel_webp_huffman_read_symbol(&code_length_tree,
                                                br,
                                                &symbol);
        if (SIXEL_FAILED(status)) {
            sixel_webp_huffman_reset(&code_length_tree, allocator);
            return status;
        }
        if (symbol >= 0 && symbol <= 15) {
            lengths[index] = (unsigned char)symbol;
            if (symbol != 0) {
                repeat_value = symbol;
            }
            ++index;
            continue;
        }

        if (symbol == 16) {
            if (!sixel_webp_bit_reader_read_bits(br, 2, &bits)) {
                sixel_webp_huffman_reset(&code_length_tree, allocator);
                sixel_helper_set_additional_message(
                    "builtin webp: truncated repeat code 16.");
                return SIXEL_BAD_INPUT;
            }
            repeat = 3 + (int)bits;
            if (index + repeat > alphabet_size) {
                sixel_webp_huffman_reset(&code_length_tree, allocator);
                sixel_helper_set_additional_message(
                    "builtin webp: code-length repeat exceeds range.");
                return SIXEL_BAD_INPUT;
            }
            for (i = 0; i < repeat; ++i) {
                lengths[index + i] = (unsigned char)repeat_value;
            }
            index += repeat;
            continue;
        }

        if (symbol == 17) {
            if (!sixel_webp_bit_reader_read_bits(br, 3, &bits)) {
                sixel_webp_huffman_reset(&code_length_tree, allocator);
                sixel_helper_set_additional_message(
                    "builtin webp: truncated repeat code 17.");
                return SIXEL_BAD_INPUT;
            }
            repeat = 3 + (int)bits;
            if (index + repeat > alphabet_size) {
                sixel_webp_huffman_reset(&code_length_tree, allocator);
                sixel_helper_set_additional_message(
                    "builtin webp: zero-run exceeds code-length range.");
                return SIXEL_BAD_INPUT;
            }
            for (i = 0; i < repeat; ++i) {
                lengths[index + i] = 0u;
            }
            index += repeat;
            continue;
        }

        if (symbol == 18) {
            if (!sixel_webp_bit_reader_read_bits(br, 7, &bits)) {
                sixel_webp_huffman_reset(&code_length_tree, allocator);
                sixel_helper_set_additional_message(
                    "builtin webp: truncated repeat code 18.");
                return SIXEL_BAD_INPUT;
            }
            repeat = 11 + (int)bits;
            if (index + repeat > alphabet_size) {
                sixel_webp_huffman_reset(&code_length_tree, allocator);
                sixel_helper_set_additional_message(
                    "builtin webp: long zero-run exceeds range.");
                return SIXEL_BAD_INPUT;
            }
            for (i = 0; i < repeat; ++i) {
                lengths[index + i] = 0u;
            }
            index += repeat;
            continue;
        }

        sixel_webp_huffman_reset(&code_length_tree, allocator);
        sixel_helper_set_additional_message(
            "builtin webp: invalid code-length symbol.");
        return SIXEL_BAD_INPUT;
    }

    sixel_webp_huffman_reset(&code_length_tree, allocator);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_read_prefix_code(sixel_webp_bit_reader_t *br,
                            sixel_webp_huffman_t *tree,
                            int alphabet_size,
                            sixel_allocator_t *allocator)
{
    unsigned char *lengths;
    uint32_t bits;
    int num_symbols;
    int is_first_8bits;
    int symbol0;
    int symbol1;
    SIXELSTATUS status;

    lengths = NULL;
    bits = 0u;
    num_symbols = 0;
    is_first_8bits = 0;
    symbol0 = 0;
    symbol1 = 0;
    status = SIXEL_OK;

    if (br == NULL || tree == NULL || alphabet_size <= 0 ||
        allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    lengths = (unsigned char *)sixel_allocator_calloc(allocator,
                                                      (size_t)alphabet_size,
                                                      sizeof(*lengths));
    if (lengths == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_calloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    if (!sixel_webp_bit_reader_read_bits(br, 1, &bits)) {
        sixel_allocator_free(allocator, lengths);
        sixel_helper_set_additional_message(
            "builtin webp: truncated prefix-code selector.");
        return SIXEL_BAD_INPUT;
    }

    if (bits != 0u) {
        if (!sixel_webp_bit_reader_read_bits(br, 1, &bits)) {
            sixel_allocator_free(allocator, lengths);
            sixel_helper_set_additional_message(
                "builtin webp: truncated simple-code symbol count.");
            return SIXEL_BAD_INPUT;
        }
        num_symbols = (int)bits + 1;

        if (!sixel_webp_bit_reader_read_bits(br, 1, &bits)) {
            sixel_allocator_free(allocator, lengths);
            sixel_helper_set_additional_message(
                "builtin webp: truncated simple-code width selector.");
            return SIXEL_BAD_INPUT;
        }
        is_first_8bits = (int)bits;

        if (!sixel_webp_bit_reader_read_bits(br,
                                             1 + 7 * is_first_8bits,
                                             &bits)) {
            sixel_allocator_free(allocator, lengths);
            sixel_helper_set_additional_message(
                "builtin webp: truncated simple-code symbol0.");
            return SIXEL_BAD_INPUT;
        }
        symbol0 = (int)bits;
        if (symbol0 < 0 || symbol0 >= alphabet_size) {
            sixel_allocator_free(allocator, lengths);
            sixel_helper_set_additional_message(
                "builtin webp: simple-code symbol0 is out of range.");
            return SIXEL_BAD_INPUT;
        }
        lengths[symbol0] = 1u;

        if (num_symbols == 2) {
            if (!sixel_webp_bit_reader_read_bits(br, 8, &bits)) {
                sixel_allocator_free(allocator, lengths);
                sixel_helper_set_additional_message(
                    "builtin webp: truncated simple-code symbol1.");
                return SIXEL_BAD_INPUT;
            }
            symbol1 = (int)bits;
            if (symbol1 < 0 || symbol1 >= alphabet_size) {
                sixel_allocator_free(allocator, lengths);
                sixel_helper_set_additional_message(
                    "builtin webp: simple-code symbol1 is out of range.");
                return SIXEL_BAD_INPUT;
            }
            lengths[symbol1] = 1u;
        }
    } else {
        status = sixel_webp_read_normal_code_lengths(br,
                                                     lengths,
                                                     alphabet_size,
                                                     allocator);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(allocator, lengths);
            return status;
        }
    }

    if (!sixel_webp_huffman_build(tree, lengths, alphabet_size, allocator)) {
        sixel_allocator_free(allocator, lengths);
        sixel_helper_set_additional_message(
            "builtin webp: invalid prefix-code tree.");
        return SIXEL_BAD_INPUT;
    }

    sixel_allocator_free(allocator, lengths);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_prefix_code_value(sixel_webp_bit_reader_t *br,
                             unsigned int prefix_code,
                             unsigned int *value)
{
    unsigned int extra_bits;
    unsigned int offset;
    uint32_t extra_value;

    extra_bits = 0u;
    offset = 0u;
    extra_value = 0u;

    if (br == NULL || value == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (prefix_code < 4u) {
        *value = prefix_code + 1u;
        return SIXEL_OK;
    }

    extra_bits = (prefix_code - 2u) >> 1;
    if (extra_bits > 24u) {
        sixel_helper_set_additional_message(
            "builtin webp: prefix code uses too many extra bits.");
        return SIXEL_BAD_INPUT;
    }

    offset = (unsigned int)((2u + (prefix_code & 1u)) << extra_bits);
    if (!sixel_webp_bit_reader_read_bits(br, (int)extra_bits, &extra_value)) {
        sixel_helper_set_additional_message(
            "builtin webp: truncated prefix-code extra bits.");
        return SIXEL_BAD_INPUT;
    }

    *value = offset + (unsigned int)extra_value + 1u;
    return SIXEL_OK;
}

static int
sixel_webp_distance_code_to_distance(unsigned int distance_code,
                                     int image_width)
{
    int xoffset;
    int yoffset;
    int distance;

    xoffset = 0;
    yoffset = 0;
    distance = 0;

    if (distance_code == 0u) {
        return 0;
    }
    if (distance_code > 120u) {
        return (int)(distance_code - 120u);
    }

    xoffset = sixel_webp_distance_map[distance_code - 1u][0];
    yoffset = sixel_webp_distance_map[distance_code - 1u][1];
    distance = xoffset + yoffset * image_width;
    if (distance < 1) {
        distance = 1;
    }
    return distance;
}

static unsigned int
sixel_webp_color_cache_hash(uint32_t argb, int cache_bits)
{
    return (unsigned int)((0x1e35a7bdu * argb) >> (32 - cache_bits));
}

static SIXELSTATUS
sixel_webp_decode_stream(sixel_webp_bit_reader_t *br,
                         int width,
                         int height,
                         int allow_meta_prefix,
                         uint32_t **ppixels,
                         sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    uint32_t bit_value;
    int use_cache;
    int cache_bits;
    int cache_size;
    uint32_t *cache;
    int use_multiple_prefix;
    int prefix_bits;
    int prefix_image_width;
    int prefix_image_height;
    uint32_t *entropy_image;
    int max_meta_prefix;
    int num_prefix_groups;
    sixel_webp_prefix_group_t *groups;
    int group_index;
    int code_index;
    int alphabet_size;
    uint32_t *pixels;
    size_t pixel_count;
    size_t pixel_pos;
    int x;
    int y;
    int entropy_index;
    int meta_prefix;
    sixel_webp_prefix_group_t *group;
    int symbol;
    uint32_t argb;
    unsigned int prefix_value;
    int length;
    int distance;
    size_t copy_index;

    status = SIXEL_OK;
    bit_value = 0u;
    use_cache = 0;
    cache_bits = 0;
    cache_size = 0;
    cache = NULL;
    use_multiple_prefix = 0;
    prefix_bits = 0;
    prefix_image_width = 1;
    prefix_image_height = 1;
    entropy_image = NULL;
    max_meta_prefix = 0;
    num_prefix_groups = 1;
    groups = NULL;
    group_index = 0;
    code_index = 0;
    alphabet_size = 0;
    pixels = NULL;
    pixel_count = 0u;
    pixel_pos = 0u;
    x = 0;
    y = 0;
    entropy_index = 0;
    meta_prefix = 0;
    group = NULL;
    symbol = 0;
    argb = 0u;
    prefix_value = 0u;
    length = 0;
    distance = 0;
    copy_index = 0u;

    if (br == NULL || ppixels == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *ppixels = NULL;

    status = sixel_webp_validate_dimensions(width, height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (!sixel_webp_bit_reader_read_bits(br, 1, &bit_value)) {
        sixel_helper_set_additional_message(
            "builtin webp: truncated color-cache flag.");
        return SIXEL_BAD_INPUT;
    }
    use_cache = (int)bit_value;
    if (use_cache != 0) {
        if (!sixel_webp_bit_reader_read_bits(br, 4, &bit_value)) {
            sixel_helper_set_additional_message(
                "builtin webp: truncated color-cache size.");
            return SIXEL_BAD_INPUT;
        }
        cache_bits = (int)bit_value;
        if (cache_bits < 1 || cache_bits > 11) {
            sixel_helper_set_additional_message(
                "builtin webp: color-cache size bits are out of range.");
            return SIXEL_BAD_INPUT;
        }
        cache_size = 1 << cache_bits;
        cache = (uint32_t *)sixel_allocator_calloc(allocator,
                                                   (size_t)cache_size,
                                                   sizeof(*cache));
        if (cache == NULL) {
            sixel_helper_set_additional_message(
                "builtin webp: sixel_allocator_calloc() failed.");
            return SIXEL_BAD_ALLOCATION;
        }
    }

    if (allow_meta_prefix != 0) {
        if (!sixel_webp_bit_reader_read_bits(br, 1, &bit_value)) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin webp: truncated meta-prefix selector.");
            goto cleanup;
        }
        use_multiple_prefix = (int)bit_value;
        if (use_multiple_prefix != 0) {
            size_t rounded;
            size_t prefix_pixel_count;

            rounded = 0u;
            prefix_pixel_count = 0u;

            if (!sixel_webp_bit_reader_read_bits(br, 3, &bit_value)) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: truncated meta-prefix size bits.");
                goto cleanup;
            }
            prefix_bits = (int)bit_value + 2;

            rounded = sixel_webp_div_round_up_size((size_t)width,
                                                   (size_t)1u << prefix_bits);
            if (rounded == 0u || rounded > (size_t)INT_MAX) {
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto cleanup;
            }
            prefix_image_width = (int)rounded;

            rounded = sixel_webp_div_round_up_size((size_t)height,
                                                   (size_t)1u << prefix_bits);
            if (rounded == 0u || rounded > (size_t)INT_MAX) {
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto cleanup;
            }
            prefix_image_height = (int)rounded;

            status = sixel_webp_decode_stream(br,
                                              prefix_image_width,
                                              prefix_image_height,
                                              0,
                                              &entropy_image,
                                              allocator);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }

            prefix_pixel_count = (size_t)prefix_image_width
                * (size_t)prefix_image_height;
            max_meta_prefix = 0;
            for (copy_index = 0u;
                 copy_index < prefix_pixel_count;
                 ++copy_index) {
                meta_prefix = (int)((entropy_image[copy_index] >> 8) & 0xffffu);
                if (meta_prefix > max_meta_prefix) {
                    max_meta_prefix = meta_prefix;
                }
            }
            if (max_meta_prefix >= INT_MAX) {
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto cleanup;
            }
            num_prefix_groups = max_meta_prefix + 1;
        }
    }

    groups = (sixel_webp_prefix_group_t *)sixel_allocator_calloc(
        allocator,
        (size_t)num_prefix_groups,
        sizeof(*groups));
    if (groups == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_calloc() failed.");
        goto cleanup;
    }

    for (group_index = 0; group_index < num_prefix_groups; ++group_index) {
        for (code_index = 0; code_index < 5; ++code_index) {
            if (code_index == 0) {
                alphabet_size = SIXEL_WEBP_LITERAL_CODES
                    + SIXEL_WEBP_LENGTH_CODES + cache_size;
            } else if (code_index == 4) {
                alphabet_size = SIXEL_WEBP_DISTANCE_CODES;
            } else {
                alphabet_size = SIXEL_WEBP_LITERAL_CODES;
            }

            status = sixel_webp_read_prefix_code(br,
                                                 &groups[group_index]
                                                      .code[code_index],
                                                 alphabet_size,
                                                 allocator);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
        }
    }

    pixel_count = (size_t)width * (size_t)height;
    pixels = (uint32_t *)sixel_allocator_malloc(allocator,
                                                pixel_count
                                                    * sizeof(*pixels));
    if (pixels == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        goto cleanup;
    }

    pixel_pos = 0u;
    while (pixel_pos < pixel_count) {
        if (use_multiple_prefix != 0) {
            x = (int)(pixel_pos % (size_t)width);
            y = (int)(pixel_pos / (size_t)width);
            entropy_index = (y >> prefix_bits) * prefix_image_width
                + (x >> prefix_bits);
            if (entropy_index < 0 ||
                entropy_index >= prefix_image_width * prefix_image_height) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: meta-prefix index is out of range.");
                goto cleanup;
            }
            meta_prefix = (int)((entropy_image[entropy_index] >> 8) & 0xffffu);
            if (meta_prefix < 0 || meta_prefix >= num_prefix_groups) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: meta-prefix code is out of range.");
                goto cleanup;
            }
            group = &groups[meta_prefix];
        } else {
            group = &groups[0];
        }

        status = sixel_webp_huffman_read_symbol(&group->code[0],
                                                br,
                                                &symbol);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }

        if (symbol < SIXEL_WEBP_LITERAL_CODES) {
            int red_symbol;
            int blue_symbol;
            int alpha_symbol;

            red_symbol = 0;
            blue_symbol = 0;
            alpha_symbol = 0;

            status = sixel_webp_huffman_read_symbol(&group->code[1],
                                                    br,
                                                    &red_symbol);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            status = sixel_webp_huffman_read_symbol(&group->code[2],
                                                    br,
                                                    &blue_symbol);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            status = sixel_webp_huffman_read_symbol(&group->code[3],
                                                    br,
                                                    &alpha_symbol);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }

            if (red_symbol < 0 || red_symbol > 255 ||
                blue_symbol < 0 || blue_symbol > 255 ||
                alpha_symbol < 0 || alpha_symbol > 255) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: literal channel symbol is out of range.");
                goto cleanup;
            }

            argb = sixel_webp_make_argb((unsigned int)alpha_symbol,
                                        (unsigned int)red_symbol,
                                        (unsigned int)symbol,
                                        (unsigned int)blue_symbol);
            pixels[pixel_pos] = argb;
            if (use_cache != 0) {
                cache[sixel_webp_color_cache_hash(argb, cache_bits)] = argb;
            }
            ++pixel_pos;
            continue;
        }

        if (symbol < SIXEL_WEBP_LITERAL_CODES + SIXEL_WEBP_LENGTH_CODES) {
            int distance_symbol;

            distance_symbol = 0;
            status = sixel_webp_prefix_code_value(
                br,
                (unsigned int)(symbol - SIXEL_WEBP_LITERAL_CODES),
                &prefix_value);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            length = (int)prefix_value;
            if (length <= 0 || length > 4096) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: backward-reference length is invalid.");
                goto cleanup;
            }

            status = sixel_webp_huffman_read_symbol(&group->code[4],
                                                    br,
                                                    &distance_symbol);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            if (distance_symbol < 0 ||
                distance_symbol >= SIXEL_WEBP_DISTANCE_CODES) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: backward-reference distance symbol "
                    "is out of range.");
                goto cleanup;
            }

            status = sixel_webp_prefix_code_value(br,
                                                  (unsigned int)distance_symbol,
                                                  &prefix_value);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            distance = sixel_webp_distance_code_to_distance(prefix_value,
                                                            width);
            if (distance <= 0 || (size_t)distance > pixel_pos) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: backward-reference distance is invalid.");
                goto cleanup;
            }
            if ((size_t)length > pixel_count - pixel_pos) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: backward-reference length exceeds image "
                    "bounds.");
                goto cleanup;
            }

            for (copy_index = 0u; copy_index < (size_t)length; ++copy_index) {
                argb = pixels[pixel_pos - (size_t)distance];
                pixels[pixel_pos] = argb;
                if (use_cache != 0) {
                    cache[sixel_webp_color_cache_hash(argb, cache_bits)] = argb;
                }
                ++pixel_pos;
            }
            continue;
        }

        if (use_cache == 0) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin webp: color-cache symbol appeared without "
                "a color cache.");
            goto cleanup;
        }

        symbol -= SIXEL_WEBP_LITERAL_CODES + SIXEL_WEBP_LENGTH_CODES;
        if (symbol < 0 || symbol >= cache_size) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin webp: color-cache index is out of range.");
            goto cleanup;
        }
        argb = cache[symbol];
        pixels[pixel_pos] = argb;
        cache[sixel_webp_color_cache_hash(argb, cache_bits)] = argb;
        ++pixel_pos;
    }

    *ppixels = pixels;
    pixels = NULL;
    status = SIXEL_OK;

cleanup:
    if (groups != NULL) {
        for (group_index = 0; group_index < num_prefix_groups;
             ++group_index) {
            for (code_index = 0; code_index < 5; ++code_index) {
                sixel_webp_huffman_reset(&groups[group_index].code[code_index],
                                         allocator);
            }
        }
        sixel_allocator_free(allocator, groups);
    }
    sixel_allocator_free(allocator, entropy_image);
    sixel_allocator_free(allocator, cache);
    sixel_allocator_free(allocator, pixels);

    return status;
}

static uint32_t
sixel_webp_predict_mode(uint32_t left,
                        uint32_t top,
                        uint32_t top_left,
                        uint32_t top_right,
                        int mode)
{
    int la;
    int lr;
    int lg;
    int lb;
    int ta;
    int tr;
    int tg;
    int tb;
    int tla;
    int tlr;
    int tlg;
    int tlb;
    int tra;
    int trr;
    int trg;
    int trb;
    int pa;
    int pr;
    int pg;
    int pb;
    int diff_left;
    int diff_top;
    int value_a;
    int value_r;
    int value_g;
    int value_b;

    la = (int)sixel_webp_argb_a(left);
    lr = (int)sixel_webp_argb_r(left);
    lg = (int)sixel_webp_argb_g(left);
    lb = (int)sixel_webp_argb_b(left);
    ta = (int)sixel_webp_argb_a(top);
    tr = (int)sixel_webp_argb_r(top);
    tg = (int)sixel_webp_argb_g(top);
    tb = (int)sixel_webp_argb_b(top);
    tla = (int)sixel_webp_argb_a(top_left);
    tlr = (int)sixel_webp_argb_r(top_left);
    tlg = (int)sixel_webp_argb_g(top_left);
    tlb = (int)sixel_webp_argb_b(top_left);
    tra = (int)sixel_webp_argb_a(top_right);
    trr = (int)sixel_webp_argb_r(top_right);
    trg = (int)sixel_webp_argb_g(top_right);
    trb = (int)sixel_webp_argb_b(top_right);
    pa = 0;
    pr = 0;
    pg = 0;
    pb = 0;
    diff_left = 0;
    diff_top = 0;
    value_a = 0;
    value_r = 0;
    value_g = 0;
    value_b = 0;

    switch (mode) {
    case 0:
        return 0xff000000u;
    case 1:
        return left;
    case 2:
        return top;
    case 3:
        return top_right;
    case 4:
        return top_left;
    case 5:
        value_a = sixel_webp_average2(sixel_webp_average2(la, tra), ta);
        value_r = sixel_webp_average2(sixel_webp_average2(lr, trr), tr);
        value_g = sixel_webp_average2(sixel_webp_average2(lg, trg), tg);
        value_b = sixel_webp_average2(sixel_webp_average2(lb, trb), tb);
        return sixel_webp_make_argb((unsigned int)value_a,
                                    (unsigned int)value_r,
                                    (unsigned int)value_g,
                                    (unsigned int)value_b);
    case 6:
        value_a = sixel_webp_average2(la, tla);
        value_r = sixel_webp_average2(lr, tlr);
        value_g = sixel_webp_average2(lg, tlg);
        value_b = sixel_webp_average2(lb, tlb);
        return sixel_webp_make_argb((unsigned int)value_a,
                                    (unsigned int)value_r,
                                    (unsigned int)value_g,
                                    (unsigned int)value_b);
    case 7:
        value_a = sixel_webp_average2(la, ta);
        value_r = sixel_webp_average2(lr, tr);
        value_g = sixel_webp_average2(lg, tg);
        value_b = sixel_webp_average2(lb, tb);
        return sixel_webp_make_argb((unsigned int)value_a,
                                    (unsigned int)value_r,
                                    (unsigned int)value_g,
                                    (unsigned int)value_b);
    case 8:
        value_a = sixel_webp_average2(tla, ta);
        value_r = sixel_webp_average2(tlr, tr);
        value_g = sixel_webp_average2(tlg, tg);
        value_b = sixel_webp_average2(tlb, tb);
        return sixel_webp_make_argb((unsigned int)value_a,
                                    (unsigned int)value_r,
                                    (unsigned int)value_g,
                                    (unsigned int)value_b);
    case 9:
        value_a = sixel_webp_average2(ta, tra);
        value_r = sixel_webp_average2(tr, trr);
        value_g = sixel_webp_average2(tg, trg);
        value_b = sixel_webp_average2(tb, trb);
        return sixel_webp_make_argb((unsigned int)value_a,
                                    (unsigned int)value_r,
                                    (unsigned int)value_g,
                                    (unsigned int)value_b);
    case 10:
        value_a = sixel_webp_average2(sixel_webp_average2(la, tla),
                                      sixel_webp_average2(ta, tra));
        value_r = sixel_webp_average2(sixel_webp_average2(lr, tlr),
                                      sixel_webp_average2(tr, trr));
        value_g = sixel_webp_average2(sixel_webp_average2(lg, tlg),
                                      sixel_webp_average2(tg, trg));
        value_b = sixel_webp_average2(sixel_webp_average2(lb, tlb),
                                      sixel_webp_average2(tb, trb));
        return sixel_webp_make_argb((unsigned int)value_a,
                                    (unsigned int)value_r,
                                    (unsigned int)value_g,
                                    (unsigned int)value_b);
    case 11:
        pa = la + ta - tla;
        pr = lr + tr - tlr;
        pg = lg + tg - tlg;
        pb = lb + tb - tlb;
        diff_left = abs(pa - la) + abs(pr - lr)
            + abs(pg - lg) + abs(pb - lb);
        diff_top = abs(pa - ta) + abs(pr - tr)
            + abs(pg - tg) + abs(pb - tb);
        return diff_left < diff_top ? left : top;
    case 12:
        value_a = sixel_webp_clamp_byte(la + ta - tla);
        value_r = sixel_webp_clamp_byte(lr + tr - tlr);
        value_g = sixel_webp_clamp_byte(lg + tg - tlg);
        value_b = sixel_webp_clamp_byte(lb + tb - tlb);
        return sixel_webp_make_argb((unsigned int)value_a,
                                    (unsigned int)value_r,
                                    (unsigned int)value_g,
                                    (unsigned int)value_b);
    case 13:
    default:
        value_a = sixel_webp_clamp_byte(
            sixel_webp_average2(la, ta)
            + (sixel_webp_average2(la, ta) - tla) / 2);
        value_r = sixel_webp_clamp_byte(
            sixel_webp_average2(lr, tr)
            + (sixel_webp_average2(lr, tr) - tlr) / 2);
        value_g = sixel_webp_clamp_byte(
            sixel_webp_average2(lg, tg)
            + (sixel_webp_average2(lg, tg) - tlg) / 2);
        value_b = sixel_webp_clamp_byte(
            sixel_webp_average2(lb, tb)
            + (sixel_webp_average2(lb, tb) - tlb) / 2);
        return sixel_webp_make_argb((unsigned int)value_a,
                                    (unsigned int)value_r,
                                    (unsigned int)value_g,
                                    (unsigned int)value_b);
    }
}

static void
sixel_webp_apply_subtract_green(uint32_t *pixels,
                                 int width,
                                 int height)
{
    size_t pixel_count;
    size_t index;
    unsigned int a;
    unsigned int r;
    unsigned int g;
    unsigned int b;

    pixel_count = 0u;
    index = 0u;
    a = 0u;
    r = 0u;
    g = 0u;
    b = 0u;

    if (pixels == NULL || width <= 0 || height <= 0) {
        return;
    }

    pixel_count = (size_t)width * (size_t)height;
    for (index = 0u; index < pixel_count; ++index) {
        a = sixel_webp_argb_a(pixels[index]);
        r = sixel_webp_argb_r(pixels[index]);
        g = sixel_webp_argb_g(pixels[index]);
        b = sixel_webp_argb_b(pixels[index]);
        r = (r + g) & 0xffu;
        b = (b + g) & 0xffu;
        pixels[index] = sixel_webp_make_argb(a, r, g, b);
    }
}

static void
sixel_webp_apply_color_transform(uint32_t *pixels,
                                 int width,
                                 int height,
                                 int size_bits,
                                 uint32_t const *transform_data,
                                 int transform_width)
{
    int x;
    int y;
    int block_index;
    uint32_t src;
    uint32_t trans;
    int a;
    int r;
    int g;
    int b;
    int g_to_r;
    int g_to_b;
    int r_to_b;

    x = 0;
    y = 0;
    block_index = 0;
    src = 0u;
    trans = 0u;
    a = 0;
    r = 0;
    g = 0;
    b = 0;
    g_to_r = 0;
    g_to_b = 0;
    r_to_b = 0;

    if (pixels == NULL || transform_data == NULL ||
        width <= 0 || height <= 0 || transform_width <= 0) {
        return;
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            block_index = (y >> size_bits) * transform_width
                + (x >> size_bits);
            src = pixels[(size_t)y * (size_t)width + (size_t)x];
            trans = transform_data[(size_t)block_index];

            a = (int)sixel_webp_argb_a(src);
            r = (int)sixel_webp_argb_r(src);
            g = (int)sixel_webp_argb_g(src);
            b = (int)sixel_webp_argb_b(src);

            g_to_r = sixel_webp_u8_to_i8(sixel_webp_argb_b(trans));
            g_to_b = sixel_webp_u8_to_i8(sixel_webp_argb_g(trans));
            r_to_b = sixel_webp_u8_to_i8(sixel_webp_argb_r(trans));

            r = (r + sixel_webp_mul_shift5(
                         g_to_r,
                         sixel_webp_u8_to_i8((unsigned int)g)))
                & 0xff;
            b = (b + sixel_webp_mul_shift5(g_to_b,
                                           sixel_webp_u8_to_i8((unsigned int)g))
                   + sixel_webp_mul_shift5(r_to_b,
                                           sixel_webp_u8_to_i8(
                                               (unsigned int)r)))
                & 0xff;

            pixels[(size_t)y * (size_t)width + (size_t)x]
                = sixel_webp_make_argb((unsigned int)a,
                                       (unsigned int)r,
                                       (unsigned int)g,
                                       (unsigned int)b);
        }
    }
}

static void
sixel_webp_apply_predictor_transform(uint32_t *pixels,
                                     int width,
                                     int height,
                                     int size_bits,
                                     uint32_t const *predictor_data,
                                     int predictor_width)
{
    int x;
    int y;
    int block_index;
    int mode;
    uint32_t residual;
    uint32_t pred;
    uint32_t left;
    uint32_t top;
    uint32_t top_left;
    uint32_t top_right;
    unsigned int a;
    unsigned int r;
    unsigned int g;
    unsigned int b;

    x = 0;
    y = 0;
    block_index = 0;
    mode = 0;
    residual = 0u;
    pred = 0u;
    left = 0u;
    top = 0u;
    top_left = 0u;
    top_right = 0u;
    a = 0u;
    r = 0u;
    g = 0u;
    b = 0u;

    if (pixels == NULL || predictor_data == NULL ||
        width <= 0 || height <= 0 || predictor_width <= 0) {
        return;
    }

    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            block_index = (y >> size_bits) * predictor_width
                + (x >> size_bits);
            mode = (int)sixel_webp_argb_g(
                predictor_data[(size_t)block_index]) & 0x0f;
            residual = pixels[(size_t)y * (size_t)width + (size_t)x];

            if (x == 0 && y == 0) {
                pred = 0xff000000u;
            } else if (y == 0) {
                pred = pixels[(size_t)y * (size_t)width + (size_t)(x - 1)];
            } else if (x == 0) {
                pred = pixels[(size_t)(y - 1) * (size_t)width + (size_t)x];
            } else {
                left = pixels[(size_t)y * (size_t)width + (size_t)(x - 1)];
                top = pixels[(size_t)(y - 1) * (size_t)width + (size_t)x];
                top_left = pixels[(size_t)(y - 1) * (size_t)width
                    + (size_t)(x - 1)];
                if (x + 1 < width) {
                    top_right = pixels[(size_t)(y - 1) * (size_t)width
                        + (size_t)(x + 1)];
                } else {
                    top_right = pixels[(size_t)(y - 1) * (size_t)width];
                }
                pred = sixel_webp_predict_mode(left,
                                               top,
                                               top_left,
                                               top_right,
                                               mode);
            }

            a = (sixel_webp_argb_a(residual) + sixel_webp_argb_a(pred)) & 0xffu;
            r = (sixel_webp_argb_r(residual) + sixel_webp_argb_r(pred)) & 0xffu;
            g = (sixel_webp_argb_g(residual) + sixel_webp_argb_g(pred)) & 0xffu;
            b = (sixel_webp_argb_b(residual) + sixel_webp_argb_b(pred)) & 0xffu;
            pixels[(size_t)y * (size_t)width + (size_t)x]
                = sixel_webp_make_argb(a, r, g, b);
        }
    }
}

static SIXELSTATUS
sixel_webp_apply_color_indexing(uint32_t **ppixels,
                                int *pwidth,
                                int output_width,
                                int height,
                                uint32_t const *color_table,
                                int color_table_size,
                                int width_bits,
                                sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    uint32_t *src;
    uint32_t *dst;
    int src_width;
    int dst_width;
    size_t dst_count;
    int x;
    int y;
    int packed_x;
    uint32_t packed_argb;
    unsigned int packed_green;
    unsigned int bits_per_index;
    unsigned int index_mask;
    unsigned int shift;
    unsigned int index;

    status = SIXEL_OK;
    src = NULL;
    dst = NULL;
    src_width = 0;
    dst_width = 0;
    dst_count = 0u;
    x = 0;
    y = 0;
    packed_x = 0;
    packed_argb = 0u;
    packed_green = 0u;
    bits_per_index = 0u;
    index_mask = 0u;
    shift = 0u;
    index = 0u;

    if (ppixels == NULL || pwidth == NULL || color_table == NULL ||
        color_table_size <= 0 || output_width <= 0 || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    src = *ppixels;
    src_width = *pwidth;
    if (src == NULL || src_width <= 0 || height <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (width_bits == 0) {
        for (y = 0; y < height; ++y) {
            for (x = 0; x < src_width; ++x) {
                index = sixel_webp_argb_g(src[(size_t)y * (size_t)src_width
                    + (size_t)x]);
                if (index < (unsigned int)color_table_size) {
                    src[(size_t)y * (size_t)src_width + (size_t)x]
                        = color_table[index];
                } else {
                    src[(size_t)y * (size_t)src_width + (size_t)x] = 0u;
                }
            }
        }
        return SIXEL_OK;
    }

    if (width_bits < 1 || width_bits > 3) {
        sixel_helper_set_additional_message(
            "builtin webp: invalid color-index width bits.");
        return SIXEL_BAD_INPUT;
    }

    dst_width = output_width;
    if (dst_width <= 0) {
        return SIXEL_BAD_INTEGER_OVERFLOW;
    }

    status = sixel_webp_validate_dimensions(dst_width, height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    dst_count = (size_t)dst_width * (size_t)height;
    dst = (uint32_t *)sixel_allocator_malloc(allocator,
                                             dst_count * sizeof(*dst));
    if (dst == NULL) {
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    bits_per_index = 8u >> width_bits;
    index_mask = (1u << bits_per_index) - 1u;

    for (y = 0; y < height; ++y) {
        for (x = 0; x < dst_width; ++x) {
            packed_x = x >> width_bits;
            packed_argb = src[(size_t)y * (size_t)src_width + (size_t)packed_x];
            packed_green = sixel_webp_argb_g(packed_argb);
            shift = (unsigned int)(x & ((1 << width_bits) - 1))
                    * bits_per_index;
            index = (packed_green >> shift) & index_mask;
            if (index < (unsigned int)color_table_size) {
                dst[(size_t)y * (size_t)dst_width + (size_t)x]
                    = color_table[index];
            } else {
                dst[(size_t)y * (size_t)dst_width + (size_t)x] = 0u;
            }
        }
    }

    sixel_allocator_free(allocator, src);
    *ppixels = dst;
    *pwidth = dst_width;
    return SIXEL_OK;
}

static void
sixel_webp_color_table_delta_decode(uint32_t *table, int table_size)
{
    int i;
    unsigned int prev_a;
    unsigned int prev_r;
    unsigned int prev_g;
    unsigned int prev_b;
    unsigned int cur_a;
    unsigned int cur_r;
    unsigned int cur_g;
    unsigned int cur_b;

    i = 0;
    prev_a = 0u;
    prev_r = 0u;
    prev_g = 0u;
    prev_b = 0u;
    cur_a = 0u;
    cur_r = 0u;
    cur_g = 0u;
    cur_b = 0u;

    if (table == NULL || table_size <= 0) {
        return;
    }

    for (i = 0; i < table_size; ++i) {
        cur_a = sixel_webp_argb_a(table[i]);
        cur_r = sixel_webp_argb_r(table[i]);
        cur_g = sixel_webp_argb_g(table[i]);
        cur_b = sixel_webp_argb_b(table[i]);
        prev_a = (prev_a + cur_a) & 0xffu;
        prev_r = (prev_r + cur_r) & 0xffu;
        prev_g = (prev_g + cur_g) & 0xffu;
        prev_b = (prev_b + cur_b) & 0xffu;
        table[i] = sixel_webp_make_argb(prev_a, prev_r, prev_g, prev_b);
    }
}

static int
sixel_webp_color_index_width_bits(int color_table_size)
{
    if (color_table_size <= 2) {
        return 3;
    }
    if (color_table_size <= 4) {
        return 2;
    }
    if (color_table_size <= 16) {
        return 1;
    }
    return 0;
}

SIXELSTATUS
sixel_webp_decode_vp8l_payload(unsigned char const *payload,
                               size_t payload_size,
                               unsigned char **prgba,
                               int *pwidth,
                               int *pheight,
                               sixel_allocator_t *allocator)
{
    /*
     * This entry point intentionally decodes from bare VP8L payload bytes so
     * container parsing and feature classification can evolve independently.
     */
    SIXELSTATUS status;
    sixel_webp_bit_reader_t br;
    uint32_t bit_value;
    int width;
    int height;
    int version;
    int has_alpha;
    int current_width;
    sixel_webp_transform_t transforms[SIXEL_WEBP_TRANSFORM_MAX];
    int seen_transform[SIXEL_WEBP_TRANSFORM_MAX];
    int transform_count;
    int transform_index;
    sixel_webp_transform_t *transform;
    uint32_t *main_pixels;
    size_t rgba_size;
    unsigned char *rgba;
    size_t pixel_count;
    size_t i;

    status = SIXEL_OK;
    memset(&br, 0, sizeof(br));
    bit_value = 0u;
    width = 0;
    height = 0;
    version = 0;
    has_alpha = 0;
    current_width = 0;
    memset(transforms, 0, sizeof(transforms));
    memset(seen_transform, 0, sizeof(seen_transform));
    transform_count = 0;
    transform_index = 0;
    transform = NULL;
    main_pixels = NULL;
    rgba_size = 0u;
    rgba = NULL;
    pixel_count = 0u;
    i = 0u;

    if (payload == NULL || prgba == NULL || pwidth == NULL ||
        pheight == NULL || allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *prgba = NULL;
    *pwidth = 0;
    *pheight = 0;

    if (payload_size < 5u) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8L payload is truncated.");
        return SIXEL_BAD_INPUT;
    }

    sixel_webp_bit_reader_init(&br, payload, payload_size);

    if (!sixel_webp_bit_reader_read_bits(&br, 8, &bit_value) ||
        bit_value != 0x2fu) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8L signature is invalid.");
        return SIXEL_BAD_INPUT;
    }

    if (!sixel_webp_bit_reader_read_bits(&br, 14, &bit_value)) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8L width is truncated.");
        return SIXEL_BAD_INPUT;
    }
    width = (int)bit_value + 1;

    if (!sixel_webp_bit_reader_read_bits(&br, 14, &bit_value)) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8L height is truncated.");
        return SIXEL_BAD_INPUT;
    }
    height = (int)bit_value + 1;

    if (!sixel_webp_bit_reader_read_bits(&br, 1, &bit_value)) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8L alpha bit is truncated.");
        return SIXEL_BAD_INPUT;
    }
    has_alpha = (int)bit_value;

    if (!sixel_webp_bit_reader_read_bits(&br, 3, &bit_value)) {
        sixel_helper_set_additional_message(
            "builtin webp: VP8L version is truncated.");
        return SIXEL_BAD_INPUT;
    }
    version = (int)bit_value;
    if (version != 0) {
        sixel_helper_set_additional_message(
            "builtin webp: unsupported VP8L version.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_webp_validate_dimensions(width, height);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    current_width = width;

    for (;;) {
        if (!sixel_webp_bit_reader_read_bits(&br, 1, &bit_value)) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin webp: transform marker is truncated.");
            goto cleanup;
        }
        if (bit_value == 0u) {
            break;
        }
        if (transform_count >= SIXEL_WEBP_TRANSFORM_MAX) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin webp: too many VP8L transforms.");
            goto cleanup;
        }

        transform = &transforms[transform_count];
        memset(transform, 0, sizeof(*transform));

        if (!sixel_webp_bit_reader_read_bits(&br, 2, &bit_value)) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin webp: transform type is truncated.");
            goto cleanup;
        }
        transform->type = (int)bit_value;
        if (transform->type < 0
            || transform->type >= SIXEL_WEBP_TRANSFORM_MAX) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin webp: transform type is out of range.");
            goto cleanup;
        }
        if (seen_transform[transform->type] != 0) {
            status = SIXEL_BAD_INPUT;
            sixel_helper_set_additional_message(
                "builtin webp: duplicate transform is invalid.");
            goto cleanup;
        }
        seen_transform[transform->type] = 1;

        transform->width_before = current_width;
        if (transform->type == 0 || transform->type == 1) {
            size_t rounded;

            rounded = 0u;
            if (!sixel_webp_bit_reader_read_bits(&br, 3, &bit_value)) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: transform size bits are truncated.");
                goto cleanup;
            }
            transform->size_bits = (int)bit_value + 2;
            rounded = sixel_webp_div_round_up_size(
                (size_t)current_width,
                (size_t)1u << transform->size_bits);
            if (rounded == 0u || rounded > (size_t)INT_MAX) {
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto cleanup;
            }
            transform->data_width = (int)rounded;
            transform->data_height = height;

            status = sixel_webp_decode_stream(&br,
                                              transform->data_width,
                                              transform->data_height,
                                              0,
                                              &transform->data,
                                              allocator);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
        } else if (transform->type == 2) {
            /* subtract-green transform has no payload */
        } else {
            size_t rounded;
            int width_bits;

            rounded = 0u;
            width_bits = 0;
            if (!sixel_webp_bit_reader_read_bits(&br, 8, &bit_value)) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: color-table size is truncated.");
                goto cleanup;
            }
            transform->color_table_size = (int)bit_value + 1;
            transform->data_width = transform->color_table_size;
            transform->data_height = 1;

            status = sixel_webp_decode_stream(&br,
                                              transform->data_width,
                                              transform->data_height,
                                              0,
                                              &transform->data,
                                              allocator);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            sixel_webp_color_table_delta_decode(transform->data,
                                                transform->color_table_size);

            width_bits = sixel_webp_color_index_width_bits(
                transform->color_table_size);
            transform->width_bits = width_bits;
            rounded = sixel_webp_div_round_up_size((size_t)current_width,
                                                   (size_t)1u << width_bits);
            if (rounded == 0u || rounded > (size_t)INT_MAX) {
                status = SIXEL_BAD_INTEGER_OVERFLOW;
                goto cleanup;
            }
            current_width = (int)rounded;
        }

        transform_count++;
    }

    status = sixel_webp_decode_stream(&br,
                                      current_width,
                                      height,
                                      1,
                                      &main_pixels,
                                      allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    for (transform_index = transform_count - 1;
         transform_index >= 0;
         --transform_index) {
        transform = &transforms[transform_index];
        if (transform->type == 2) {
            sixel_webp_apply_subtract_green(main_pixels,
                                            current_width,
                                            height);
        } else if (transform->type == 1) {
            if (transform->data == NULL ||
                transform->data_width <= 0 ||
                transform->width_before != current_width) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: color transform metadata is invalid.");
                goto cleanup;
            }
            sixel_webp_apply_color_transform(main_pixels,
                                             current_width,
                                             height,
                                             transform->size_bits,
                                             transform->data,
                                             transform->data_width);
        } else if (transform->type == 0) {
            if (transform->data == NULL ||
                transform->data_width <= 0 ||
                transform->width_before != current_width) {
                status = SIXEL_BAD_INPUT;
                sixel_helper_set_additional_message(
                    "builtin webp: predictor transform metadata is "
                    "invalid.");
                goto cleanup;
            }
            sixel_webp_apply_predictor_transform(main_pixels,
                                                 current_width,
                                                 height,
                                                 transform->size_bits,
                                                 transform->data,
                                                 transform->data_width);
        } else {
            status = sixel_webp_apply_color_indexing(&main_pixels,
                                                     &current_width,
                                                     transform->width_before,
                                                     height,
                                                     transform->data,
                                                     transform->
                                                     color_table_size,
                                                     transform->width_bits,
                                                     allocator);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
        }
    }

    if (current_width != width) {
        status = SIXEL_BAD_INPUT;
        sixel_helper_set_additional_message(
            "builtin webp: transform width restoration failed.");
        goto cleanup;
    }

    pixel_count = (size_t)width * (size_t)height;
    if (pixel_count > SIZE_MAX / 4u) {
        status = SIXEL_BAD_INTEGER_OVERFLOW;
        goto cleanup;
    }
    rgba_size = pixel_count * 4u;
    rgba = (unsigned char *)sixel_allocator_malloc(allocator, rgba_size);
    if (rgba == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        sixel_helper_set_additional_message(
            "builtin webp: sixel_allocator_malloc() failed.");
        goto cleanup;
    }

    for (i = 0u; i < pixel_count; ++i) {
        rgba[i * 4u + 0u] = (unsigned char)sixel_webp_argb_r(main_pixels[i]);
        rgba[i * 4u + 1u] = (unsigned char)sixel_webp_argb_g(main_pixels[i]);
        rgba[i * 4u + 2u] = (unsigned char)sixel_webp_argb_b(main_pixels[i]);
        rgba[i * 4u + 3u] = (unsigned char)sixel_webp_argb_a(main_pixels[i]);
    }

    *prgba = rgba;
    *pwidth = width;
    *pheight = height;
    rgba = NULL;

    if (has_alpha != 0) {
        /* Keep header alpha flag consumed for future diagnostics. */
        (void)has_alpha;
    }

cleanup:
    for (transform_index = 0;
         transform_index < transform_count;
         ++transform_index) {
        sixel_allocator_free(allocator, transforms[transform_index].data);
    }
    sixel_allocator_free(allocator, main_pixels);
    sixel_allocator_free(allocator, rgba);

    return status;
}



/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
