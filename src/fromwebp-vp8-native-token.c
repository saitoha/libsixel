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

#if HAVE_STDINT_H
# include <stdint.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif

#include "compat_stub.h"
#include "fromwebp-vp8-native-internal.h"
#include "fromwebp-vp8-tables.h"
#include "loader-common.h"

static unsigned int const sixel_webp_vp8_zigzag[16] =
    {0u, 1u, 4u, 8u, 5u, 2u, 3u, 6u,
     9u, 12u, 13u, 10u, 7u, 11u, 14u, 15u};
static unsigned int const sixel_webp_vp8_coeff_band[17] =
    {0u, 1u, 2u, 3u, 6u, 4u, 5u, 6u, 6u,
     6u, 6u, 6u, 6u, 6u, 6u, 7u, 0u};

static unsigned int const sixel_webp_vp8_cat3_prob[3] =
    {173u, 148u, 140u};
static unsigned int const sixel_webp_vp8_cat4_prob[4] =
    {176u, 155u, 140u, 135u};
static unsigned int const sixel_webp_vp8_cat5_prob[5] =
    {180u, 157u, 141u, 134u, 130u};
static unsigned int const sixel_webp_vp8_cat6_prob[11] =
    {254u, 254u, 243u, 230u, 196u, 177u, 153u, 140u, 133u, 130u, 129u};

static void
sixel_webp_vp8_bool_load_final_byte(sixel_webp_vp8_bool_decoder_t *decoder)
{
    if (decoder == NULL || decoder->buffer == NULL) {
        return;
    }

    if (decoder->position < decoder->size) {
        decoder->bits += 8;
        decoder->value =
            (uint32_t)decoder->buffer[decoder->position]
            | (decoder->value << 8);
        decoder->position++;
    } else if (decoder->eof == 0) {
        decoder->value <<= 8;
        decoder->bits += 8;
        decoder->eof = 1;
    } else {
        decoder->bits = 0;
    }
}

static void
sixel_webp_vp8_bool_fill(sixel_webp_vp8_bool_decoder_t *decoder)
{
    uint32_t bits;

    bits = 0u;
    if (decoder == NULL || decoder->buffer == NULL) {
        return;
    }

    if (decoder->position + 3u <= decoder->size) {
        bits = (uint32_t)decoder->buffer[decoder->position] << 16;
        bits |= (uint32_t)decoder->buffer[decoder->position + 1u] << 8;
        bits |= (uint32_t)decoder->buffer[decoder->position + 2u];
        decoder->position += 3u;
        decoder->value = bits | (decoder->value << 24);
        decoder->bits += 24;
    } else {
        sixel_webp_vp8_bool_load_final_byte(decoder);
    }
}

SIXELSTATUS
sixel_webp_vp8_bool_init(sixel_webp_vp8_bool_decoder_t *decoder,
                         unsigned char const *buffer,
                         size_t size)
{
    if (decoder == NULL || buffer == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    decoder->buffer = buffer;
    decoder->size = size;
    decoder->position = 0u;
    decoder->value = 0u;
    decoder->range = 254u;
    decoder->bits = -8;
    decoder->eof = 0;
    sixel_webp_vp8_bool_fill(decoder);

    return SIXEL_OK;
}

SIXELSTATUS
sixel_webp_vp8_bool_read(sixel_webp_vp8_bool_decoder_t *decoder,
                         unsigned int probability,
                         int *pbit)
{
    SIXELSTATUS status;
    unsigned int split;
    unsigned int value;
    unsigned int range;
    unsigned int tmp;
    unsigned int shift;
    int bit;

    status = SIXEL_OK;
    split = 0u;
    value = 0u;
    range = 0u;
    shift = 0u;
    bit = 0;
    if (decoder == NULL || pbit == NULL || probability > 255u) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (decoder->bits < 0) {
        sixel_webp_vp8_bool_fill(decoder);
        if (decoder->bits < 0) {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 bitstream partition is truncated.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
    }

    range = decoder->range;
    split = (range * probability) >> 8;
    value = (unsigned int)(decoder->value >> (unsigned int)decoder->bits);
    if (value > split) {
        range -= split;
        decoder->value -=
            (uint32_t)(split + 1u) << (unsigned int)decoder->bits;
        bit = 1;
    } else {
        range = split + 1u;
        bit = 0;
    }

    tmp = range;
    shift = 0u;
    while (tmp > 1u) {
        tmp >>= 1u;
        shift++;
    }
    shift = 7u - shift;
    range <<= shift;
    decoder->bits -= (int)shift;
    decoder->range = range - 1u;
    *pbit = bit;

end:
    return status;
}

SIXELSTATUS
sixel_webp_vp8_bool_read_literal(sixel_webp_vp8_bool_decoder_t *decoder,
                                 unsigned int nbits,
                                 unsigned int *pvalue)
{
    SIXELSTATUS status;
    unsigned int value;
    unsigned int i;
    int bit;

    status = SIXEL_OK;
    value = 0u;
    i = 0u;
    bit = 0;
    if (decoder == NULL || pvalue == NULL || nbits > 24u) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (i = 0u; i < nbits; ++i) {
        status = sixel_webp_vp8_bool_read(decoder,
                                          SIXEL_WEBP_VP8_BOOL_BASE_PROB,
                                          &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        value = (value << 1) | (unsigned int)bit;
    }

    *pvalue = value;
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_webp_vp8_bool_read_signed_value(sixel_webp_vp8_bool_decoder_t *decoder,
                                      unsigned int value,
                                      int *psigned)
{
    unsigned int split;
    unsigned int dec_value;
    int32_t mask;
    int signed_value;

    split = 0u;
    dec_value = 0u;
    mask = 0;
    signed_value = 0;
    if (decoder == NULL || psigned == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (decoder->bits < 0) {
        sixel_webp_vp8_bool_fill(decoder);
        if (decoder->bits < 0) {
            sixel_helper_set_additional_message(
                "builtin webp: VP8 bitstream partition is truncated.");
            return SIXEL_BAD_INPUT;
        }
    }

    split = decoder->range >> 1u;
    dec_value = (unsigned int)(decoder->value >> (unsigned int)decoder->bits);
    mask = (int32_t)(split - dec_value) >> 31;
    decoder->bits--;
    decoder->range = decoder->range + (unsigned int)mask;
    decoder->range |= 1u;
    decoder->value -= (uint32_t)((split + 1u) & (unsigned int)mask)
        << (unsigned int)(decoder->bits + 1);

    if (mask != 0) {
        signed_value = -(int)value;
    } else {
        signed_value = (int)value;
    }

    *psigned = signed_value;
    return SIXEL_OK;
}

SIXELSTATUS
sixel_webp_vp8_decode_coeff_block(
    sixel_webp_vp8_bool_decoder_t *decoder,
    unsigned char const probs[SIXEL_WEBP_VP8_COEFF_BANDS]
                             [SIXEL_WEBP_VP8_PREV_COEFF_CONTEXTS]
                             [SIXEL_WEBP_VP8_COEFF_NODES],
    unsigned int start_coeff,
    unsigned int coeff_context,
    int16_t coeffs[SIXEL_WEBP_VP8_BLOCK_COEFFS],
    unsigned int *peob)
{
    SIXELSTATUS status;
    unsigned int n;
    unsigned int value;
    unsigned int j;
    unsigned int cat;
    unsigned int bit0;
    unsigned int bit1;
    unsigned int k;
    unsigned int cat_len;
    int bit;
    int signed_value;
    unsigned char const *p;
    unsigned int const *cat_prob;

    status = SIXEL_OK;
    n = 0u;
    value = 0u;
    j = 0u;
    cat = 0u;
    bit0 = 0u;
    bit1 = 0u;
    k = 0u;
    cat_len = 0u;
    bit = 0;
    signed_value = 0;
    p = NULL;
    cat_prob = NULL;
    if (decoder == NULL || probs == NULL || coeffs == NULL || peob == NULL ||
        start_coeff > SIXEL_WEBP_VP8_BLOCK_COEFFS ||
        coeff_context > 2u) {
        return SIXEL_BAD_ARGUMENT;
    }

    memset(coeffs, 0, sizeof(int16_t) * SIXEL_WEBP_VP8_BLOCK_COEFFS);
    *peob = start_coeff;
    if (start_coeff >= SIXEL_WEBP_VP8_BLOCK_COEFFS) {
        return SIXEL_OK;
    }

    n = start_coeff;
    p = probs[n][coeff_context];
    while (n < SIXEL_WEBP_VP8_BLOCK_COEFFS) {
        status = sixel_webp_vp8_bool_read(decoder, p[0], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (bit == 0) {
            *peob = n;
            return SIXEL_OK;
        }

        status = sixel_webp_vp8_bool_read(decoder, p[1], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        while (bit == 0) {
            ++n;
            if (n == SIXEL_WEBP_VP8_BLOCK_COEFFS) {
                *peob = n;
                return SIXEL_OK;
            }
            p = probs[sixel_webp_vp8_coeff_band[n]][0];
            status = sixel_webp_vp8_bool_read(decoder, p[1], &bit);
            if (SIXEL_FAILED(status)) {
                return status;
            }
        }

        status = sixel_webp_vp8_bool_read(decoder, p[2], &bit);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        if (bit == 0) {
            value = 1u;
            p = probs[sixel_webp_vp8_coeff_band[n + 1u]][1];
        } else {
            status = sixel_webp_vp8_bool_read(decoder, p[3], &bit);
            if (SIXEL_FAILED(status)) {
                return status;
            }
            if (bit == 0) {
                status = sixel_webp_vp8_bool_read(decoder, p[4], &bit);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                if (bit == 0) {
                    value = 2u;
                } else {
                    status = sixel_webp_vp8_bool_read(decoder, p[5], &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    value = 3u + (unsigned int)bit;
                }
            } else {
                status = sixel_webp_vp8_bool_read(decoder, p[6], &bit);
                if (SIXEL_FAILED(status)) {
                    return status;
                }
                if (bit == 0) {
                    status = sixel_webp_vp8_bool_read(decoder, p[7], &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    if (bit == 0) {
                        status = sixel_webp_vp8_bool_read(
                            decoder, 159u, &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        value = 5u + (unsigned int)bit;
                    } else {
                        status = sixel_webp_vp8_bool_read(
                            decoder, 165u, &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        value = 7u + ((unsigned int)bit << 1);
                        status = sixel_webp_vp8_bool_read(
                            decoder, 145u, &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        value += (unsigned int)bit;
                    }
                } else {
                    status = sixel_webp_vp8_bool_read(decoder, p[8], &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    bit1 = (unsigned int)bit;
                    status = sixel_webp_vp8_bool_read(
                        decoder, p[9u + bit1], &bit);
                    if (SIXEL_FAILED(status)) {
                        return status;
                    }
                    bit0 = (unsigned int)bit;
                    cat = (bit1 << 1) + bit0;
                    value = 0u;
                    switch (cat) {
                    case 0u:
                        cat_prob = sixel_webp_vp8_cat3_prob;
                        cat_len = 3u;
                        break;
                    case 1u:
                        cat_prob = sixel_webp_vp8_cat4_prob;
                        cat_len = 4u;
                        break;
                    case 2u:
                        cat_prob = sixel_webp_vp8_cat5_prob;
                        cat_len = 5u;
                        break;
                    default:
                        cat_prob = sixel_webp_vp8_cat6_prob;
                        cat_len = 11u;
                        break;
                    }
                    for (k = 0u; k < cat_len; ++k) {
                        status = sixel_webp_vp8_bool_read(
                            decoder, cat_prob[k], &bit);
                        if (SIXEL_FAILED(status)) {
                            return status;
                        }
                        value = (value << 1) + (unsigned int)bit;
                    }
                    value += 3u + (8u << cat);
                }
            }
            p = probs[sixel_webp_vp8_coeff_band[n + 1u]][2];
        }

        j = sixel_webp_vp8_zigzag[n];
        if (j >= SIXEL_WEBP_VP8_BLOCK_COEFFS) {
            return SIXEL_BAD_INPUT;
        }
        status = sixel_webp_vp8_bool_read_signed_value(decoder,
                                                       value,
                                                       &signed_value);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        coeffs[j] = (int16_t)signed_value;

        ++n;
    }

    *peob = SIXEL_WEBP_VP8_BLOCK_COEFFS;
    return SIXEL_OK;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
