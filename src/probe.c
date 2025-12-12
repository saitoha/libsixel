/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
 */

#include "config.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "probe.h"

SIXELAPI SIXELSTATUS
sixel_probe_find_dcs_start(uint8_t const *data,
                           size_t len,
                           size_t *out_offset)
{
    size_t i;

    if (data == NULL || out_offset == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (i = 0; i < len; ++i) {
        if (data[i] == 0x90) {
            *out_offset = i;
            return SIXEL_OK;
        }
        if (data[i] == 0x1b && i + 1 < len && data[i + 1] == 'P') {
            *out_offset = i;
            return SIXEL_OK;
        }
    }

    return SIXEL_FALSE;
}

static int
sixel_probe_is_utf8_lead(unsigned char ch, unsigned int *u8len)
{
    if (ch >= 0xc2 && ch <= 0xdf) {
        *u8len = 1;
        return 1;
    }
    if (ch >= 0xe0 && ch <= 0xef && ch != 0xed) {
        *u8len = 2;
        return 1;
    }
    if (ch >= 0xf0 && ch <= 0xf4) {
        *u8len = 3;
        return 1;
    }

    return 0;
}

static int
sixel_probe_clamp_index(int idx, int max)
{
    if (idx + 1 < max) {
        return idx + 1;
    }

    return max;
}

SIXELAPI SIXELSTATUS
sixel_parse_header(unsigned char const *ibuf,
                   size_t headsize,
                   unsigned int **pparams,
                   size_t *pparamsize,
                   sixel_allocator_t *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    enum {
        STATE_GROUND = 0,
        STATE_ESC = 1,
        STATE_ESC_INTERMEDIATE = 2,
        STATE_CSI_PARAMETER = 3,
        STATE_CSI_INTERMEDIATE = 4,
        STATE_SS = 5,
        STATE_OSC = 6,
        STATE_DCS_PARAMETER = 7,
        STATE_DCS_INTERMEDIATE = 8,
        STATE_SOS = 9,
        STATE_PM = 10,
        STATE_APC = 11,
        STATE_UTF8 = 12,
        STATE_OSC_UTF8 = 13
    } state = STATE_GROUND;
    unsigned char const *p;
    unsigned int params[1024];
    int prm = 0;
    int prm_max = 255;
    int idx = 0;
    int idx_max = (int)(sizeof(params) / sizeof(params[0]));
    unsigned int ibytes = 0;
    unsigned int u8len = 0;

    if (ibuf == NULL || pparams == NULL || pparamsize == NULL ||
            allocator == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *pparams = NULL;
    *pparamsize = 0;

    for (p = ibuf; p < ibuf + headsize; ++p) {
        switch (state) {
        case STATE_GROUND:
            if (*p == 0x1b) {
                state = STATE_ESC;
            } else if (*p == 0x90) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_DCS_PARAMETER;
            } else if (*p == 0x98) {
                state = STATE_SOS;
            } else if (*p == 0x9b) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_CSI_PARAMETER;
            } else if (*p == 0x9e) {
                state = STATE_PM;
            } else if (*p == 0x9f) {
                state = STATE_APC;
            } else if (sixel_probe_is_utf8_lead(*p, &u8len)) {
                state = STATE_UTF8;
            }
            break;
        case STATE_ESC:
            if (*p <= 0x17) {
                /* ignore */
            } else if (*p == 0x18 || *p == 0x1a) {
                state = STATE_GROUND;
            } else if (*p == 0x1b) {
                /* ignore */
            } else if (*p >= 0x20 && *p <= 0x2f) {
                ibytes = (ibytes << 8) | *p;
                state = STATE_ESC_INTERMEDIATE;
            } else if (*p >= 0x30 && *p <= 0x4d) {
                state = STATE_GROUND;
            } else if (*p == 0x4e || *p == 0x4f) {
                state = STATE_SS;
            } else if (*p == 0x50) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_DCS_PARAMETER;
            } else if (*p >= 0x51 && *p <= 0x57) {
                state = STATE_GROUND;
            } else if (*p == 0x58) {
                state = STATE_SOS;
            } else if (*p == 0x59 || *p == 0x5a) {
                state = STATE_GROUND;
            } else if (*p == 0x5b) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_CSI_PARAMETER;
            } else if (*p == 0x5c) {
                state = STATE_GROUND;
            } else if (*p == 0x5d) {
                state = STATE_OSC;
            } else if (*p == 0x5e) {
                state = STATE_PM;
            } else if (*p == 0x5f) {
                state = STATE_APC;
            } else if (*p >= 0x60 && *p <= 0x7e) {
                state = STATE_GROUND;
            } else if (*p == 0x7f) {
                /* ignore */
            } else if (*p == 0x90) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_DCS_PARAMETER;
            } else if (*p == 0x98) {
                state = STATE_SOS;
            } else if (*p == 0x9b) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_CSI_PARAMETER;
            } else if (*p == 0x9e) {
                state = STATE_PM;
            } else if (*p == 0x9f) {
                state = STATE_APC;
            } else if (sixel_probe_is_utf8_lead(*p, &u8len)) {
                state = STATE_UTF8;
            } else {
                state = STATE_GROUND;
            }
            break;
        case STATE_CSI_PARAMETER:
            if (*p <= 0x17) {
                /* ignore */
            } else if (*p == 0x18 || *p == 0x1a) {
                state = STATE_GROUND;
            } else if (*p == 0x1b) {
                state = STATE_ESC;
            } else if (*p >= 0x1c && *p <= 0x1f) {
                /* ignore */
            } else if (*p >= 0x20 && *p <= 0x2f) {
                idx = sixel_probe_clamp_index(idx, idx_max);
                params[idx] = (unsigned int)prm;
                prm = 0;
                ibytes = (ibytes << 8) | *p;
                state = STATE_CSI_INTERMEDIATE;
            } else if (*p >= '0' && *p <= '9') {
                prm = prm * 10 + (int)(*p - '0');
                if (prm > prm_max) {
                    prm = prm_max;
                }
            } else if (*p == ':') {
                ibytes = ibytes << 8;
            } else if (*p == ';') {
                idx = sixel_probe_clamp_index(idx, idx_max);
                params[idx] = (unsigned int)prm;
                prm = 0;
            } else if (*p >= 0x3c && *p <= 0x3f) {
                ibytes = (ibytes << 8) | *p;
            } else if (*p >= 0x40 && *p <= 0x7e) {
                idx = sixel_probe_clamp_index(idx, idx_max);
                params[idx] = (unsigned int)prm;
                ibytes = (ibytes << 8) | *p;
                state = STATE_GROUND;
            } else if (*p == 0x7f) {
                /* ignore */
            } else if (*p == 0x90) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_DCS_PARAMETER;
            } else if (*p == 0x98) {
                state = STATE_SOS;
            } else if (*p == 0x9b) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_CSI_PARAMETER;
            } else if (*p == 0x9e) {
                state = STATE_PM;
            } else if (*p == 0x9f) {
                state = STATE_APC;
            } else if (sixel_probe_is_utf8_lead(*p, &u8len)) {
                state = STATE_UTF8;
            } else {
                state = STATE_GROUND;
            }
            break;
        case STATE_CSI_INTERMEDIATE:
            if (*p <= 0x17) {
                /* ignore */
            } else if (*p == 0x18 || *p == 0x1a) {
                state = STATE_GROUND;
            } else if (*p == 0x1b) {
                state = STATE_ESC;
            } else if (*p >= 0x1c && *p <= 0x1f) {
                /* ignore */
            } else if (*p >= 0x20 && *p <= 0x2f) {
                ibytes = (ibytes << 8) | *p;
            } else if (*p >= 0x30 && *p <= 0x3f) {
                state = STATE_GROUND;
            } else if (*p >= 0x40 && *p <= 0x7e) {
                ibytes = (ibytes << 8) | *p;
                state = STATE_GROUND;
            } else if (*p == 0x7f) {
                /* ignore */
            } else if (*p == 0x90) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_DCS_PARAMETER;
            } else if (*p == 0x98) {
                state = STATE_SOS;
            } else if (*p == 0x9b) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_CSI_PARAMETER;
            } else if (*p == 0x9e) {
                state = STATE_PM;
            } else if (*p == 0x9f) {
                state = STATE_APC;
            } else if (sixel_probe_is_utf8_lead(*p, &u8len)) {
                state = STATE_UTF8;
            } else {
                state = STATE_GROUND;
            }
            break;
        case STATE_ESC_INTERMEDIATE:
            if (*p <= 0x17) {
                /* ignore */
            } else if (*p == 0x18 || *p == 0x1a) {
                state = STATE_GROUND;
            } else if (*p == 0x1b) {
                state = STATE_ESC;
            } else if (*p >= 0x1c && *p <= 0x1f) {
                /* ignore */
            } else if (*p >= 0x20 && *p <= 0x2f) {
                ibytes = (ibytes << 8) | *p;
            } else if (*p >= 0x30 && *p <= 0x3f) {
                state = STATE_GROUND;
            } else if (*p >= 0x40 && *p <= 0x7e) {
                ibytes = (ibytes << 8) | *p;
                state = STATE_GROUND;
            } else if (*p == 0x90) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_DCS_PARAMETER;
            } else if (*p == 0x98) {
                state = STATE_SOS;
            } else if (*p == 0x9b) {
                ibytes = 0;
                prm = 0;
                idx = 0;
                state = STATE_CSI_PARAMETER;
            } else if (*p == 0x9e) {
                state = STATE_PM;
            } else if (*p == 0x9f) {
                state = STATE_APC;
            } else if (sixel_probe_is_utf8_lead(*p, &u8len)) {
                state = STATE_UTF8;
            } else {
                state = STATE_GROUND;
            }
            break;
        case STATE_OSC:
            if (*p == 0x07) {
                state = STATE_GROUND;
            } else if (*p == 0x08) {
                state = STATE_GROUND;
            } else if (*p == 0x18 || *p == 0x1a) {
                state = STATE_GROUND;
            } else if (*p == 0x1b) {
                state = STATE_ESC;
            } else if (*p == 0x9c) {
                state = STATE_GROUND;
            } else if (sixel_probe_is_utf8_lead(*p, &u8len)) {
                state = STATE_OSC_UTF8;
            }
            break;
        case STATE_DCS_PARAMETER:
            if (*p <= 0x17) {
                /* ignore */
            } else if (*p == 0x18 || *p == 0x1a) {
                state = STATE_GROUND;
            } else if (*p == 0x1b) {
                state = STATE_ESC;
            } else if (*p >= 0x1c && *p <= 0x1f) {
                /* ignore */
            } else if (*p >= 0x20 && *p <= 0x2f) {
                ibytes = (ibytes << 8) | *p;
            } else if (*p >= 0x30 && *p <= 0x3f) {
                state = STATE_GROUND;
            } else if (*p >= 0x40 && *p <= 0x7e) {
                ibytes = (ibytes << 8) | *p;
                state = STATE_GROUND;
                if (ibytes == 'q') {
                    *pparamsize = (size_t)idx;
                    if (idx > 0) {
                        *pparams = sixel_allocator_malloc(
                            allocator,
                            sizeof(unsigned int) * (size_t)idx);
                        if (*pparams == NULL) {
                            status = SIXEL_BAD_ALLOCATION;
                            goto end;
                        }
                        memcpy(*pparams,
                               params,
                               sizeof(unsigned int) * (size_t)idx);
                    }
                    status = SIXEL_OK;
                    goto end;
                }
            } else if (*p == 0x9c) {
                state = STATE_GROUND;
            } else {
                state = STATE_GROUND;
            }
            break;
        case STATE_SOS:
        case STATE_PM:
        case STATE_APC:
            if (*p == 0x18 || *p == 0x1a) {
                state = STATE_GROUND;
            } else if (*p == 0x1b) {
                state = STATE_ESC;
            } else if (*p == 0x9c) {
                state = STATE_GROUND;
            }
            break;
        case STATE_SS:
            if (*p == 0x1b) {
                state = STATE_ESC;
            } else {
                state = STATE_GROUND;
            }
            break;
        case STATE_UTF8:
            if (*p >= 0x80 && *p <= 0xbf) {
                if (--u8len == 0) {
                    state = STATE_GROUND;
                }
            } else {
                --p;
                state = STATE_GROUND;
            }
            break;
        case STATE_OSC_UTF8:
            if (*p >= 0x80 && *p <= 0xbf) {
                if (--u8len == 0) {
                    state = STATE_OSC;
                }
            } else {
                --p;
                state = STATE_OSC;
            }
            break;
        case STATE_DCS_INTERMEDIATE:
            /* not used in original parser */
            state = STATE_GROUND;
            break;
        }
    }

end:
    return status;
}

SIXELAPI SIXELSTATUS
sixel_probe_is_probable(uint8_t const *data, size_t len)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    unsigned int *params;
    size_t paramsize;
    size_t offset;

    if (data == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = sixel_probe_find_dcs_start(data, len, &offset);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = sixel_allocator_new(
        &allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    params = NULL;
    paramsize = 0;
    status = sixel_parse_header(data + offset,
                                len - offset,
                                &params,
                                &paramsize,
                                allocator);
    if (params != NULL) {
        sixel_allocator_free(allocator, params);
    }
    sixel_allocator_unref(allocator);

    return status;
}

