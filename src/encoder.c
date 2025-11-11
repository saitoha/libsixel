/* SPDX-License-Identifier: MIT AND BSD-3-Clause
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
 * -------------------------------------------------------------------------------
 * Portions of this file(sixel_encoder_emit_drcsmmv2_chars) are derived from
 * mlterm's drcssixel.c.
 *
 * Copyright (c) Araki Ken(arakiken@users.sourceforge.net)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of any author may not be used to endorse or promote
 *    products derived from this software without their specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "config.h"
#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

# if HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#if HAVE_UNISTD_H
# include <unistd.h>
#elif HAVE_SYS_UNISTD_H
# include <sys/unistd.h>
#endif  /* HAVE_SYS_UNISTD_H */
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif  /* HAVE_SYS_TYPES_H */
#if HAVE_INTTYPES_H
# include <inttypes.h>
#endif  /* HAVE_INTTYPES_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif  /* HAVE_SYS_STAT_H */
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#elif HAVE_TIME_H
# include <time.h>
#endif  /* HAVE_SYS_TIME_H HAVE_TIME_H */
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif  /* HAVE_SYS_IOCTL_H */
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif  /* HAVE_FCNTL_H */
#if HAVE_ERRNO_H
# include <errno.h>
#endif  /* HAVE_ERRNO_H */
#if HAVE_CTYPE_H
# include <ctype.h>
#endif  /* HAVE_CTYPE_H */
#if HAVE_LIMITS_H
# include <limits.h>
#endif  /* HAVE_LIMITS_H */

#include <sixel.h>
#include "loader.h"
#include "assessment.h"
#include "tty.h"
#include "encoder.h"
#include "output.h"
#include "dither.h"
#include "frame.h"
#include "rgblookup.h"
#include "clipboard.h"
#include "compat_stub.h"

static void clipboard_select_format(char *dest,
                                    size_t dest_size,
                                    char const *format,
                                    char const *fallback);
static SIXELSTATUS clipboard_create_spool(sixel_allocator_t *allocator,
                                          char const *prefix,
                                          char **path_out,
                                          int *fd_out);
static SIXELSTATUS clipboard_write_file(char const *path,
                                        unsigned char const *data,
                                        size_t size);
static SIXELSTATUS clipboard_read_file(char const *path,
                                       unsigned char **data,
                                       size_t *size);

#if defined(_WIN32)

# include <windows.h>
# if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#  include <io.h>
# endif
# if defined(_MSC_VER)
#   include <time.h>
# endif

# if defined(CLOCKS_PER_SEC)
#  undef CLOCKS_PER_SEC
# endif
# define CLOCKS_PER_SEC 1000

# if !defined(HAVE_NANOSLEEP)
# define HAVE_NANOSLEEP_WIN 1
static int
nanosleep_win(
    struct timespec const *req,
    struct timespec *rem)
{
    LONGLONG nanoseconds;
    LARGE_INTEGER dueTime;
    HANDLE timer;

    if (req == NULL || req->tv_sec < 0 || req->tv_nsec < 0 ||
        req->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return (-1);
    }

    /* Convert to 100-nanosecond intervals (Windows FILETIME units) */
    nanoseconds = req->tv_sec * 1000000000LL + req->tv_nsec;
    dueTime.QuadPart = -(nanoseconds / 100); /* Negative for relative time */

    timer = CreateWaitableTimer(NULL, TRUE, NULL);
    if (timer == NULL) {
        errno = EFAULT;  /* Approximate error */
        return (-1);
    }

    if (! SetWaitableTimer(timer, &dueTime, 0, NULL, NULL, FALSE)) {
        (void) CloseHandle(timer);
        errno = EFAULT;
        return (-1);
    }

    (void) WaitForSingleObject(timer, INFINITE);
    (void) CloseHandle(timer);

    /* No interruption handling, so rem is unchanged */
    if (rem != NULL) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }

    return (0);
}
# endif  /* HAVE_NANOSLEEP */

# if !defined(HAVE_CLOCK)
# define HAVE_CLOCK_WIN 1
static sixel_clock_t
clock_win(void)
{
    FILETIME ct, et, kt, ut;
    ULARGE_INTEGER u, k;

    if (! GetProcessTimes(GetCurrentProcess(), &ct, &et, &kt, &ut)) {
        return (sixel_clock_t)(-1);
    }
    u.LowPart = ut.dwLowDateTime; u.HighPart = ut.dwHighDateTime;
    k.LowPart = kt.dwLowDateTime; k.HighPart = kt.dwHighDateTime;
    /* 100ns -> ms */
    return (sixel_clock_t)((u.QuadPart + k.QuadPart) / 10000ULL);
}
# endif  /* HAVE_CLOCK */

#endif /* _WIN32 */


/*
 * Prefix matcher roadmap:
 *
 *   +---------+-------------------+
 *   | input   | decision           |
 *   +---------+-------------------+
 *   | "ave"   | average            |
 *   | "a"     | ambiguous (auto?)  |
 *   +---------+-------------------+
 *
 * The helper walks the choice table once, collecting prefixes and
 * checking whether a unique destination emerges.  Ambiguous prefixes
 * bubble up so the caller can craft a friendly diagnostic.
 */
typedef struct sixel_option_choice {
    char const *name;
    int value;
} sixel_option_choice_t;

typedef enum sixel_option_choice_result {
    SIXEL_OPTION_CHOICE_MATCH = 0,
    SIXEL_OPTION_CHOICE_AMBIGUOUS = 1,
    SIXEL_OPTION_CHOICE_NONE = 2
} sixel_option_choice_result_t;

static sixel_option_choice_result_t
sixel_match_option_choice(
    char const *value,
    sixel_option_choice_t const *choices,
    size_t choice_count,
    int *matched_value,
    char *diagnostic,
    size_t diagnostic_size)
{
    size_t index;
    size_t value_length;
    int candidate_index;
    size_t match_count;
    int base_value;
    int base_value_set;
    int ambiguous_values;
    size_t diag_length;
    size_t copy_length;

    if (diagnostic != NULL && diagnostic_size > 0u) {
        diagnostic[0] = '\0';
    }
    if (value == NULL) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    value_length = strlen(value);
    if (value_length == 0u) {
        return SIXEL_OPTION_CHOICE_NONE;
    }

    index = 0u;
    candidate_index = (-1);
    match_count = 0u;
    base_value = 0;
    base_value_set = 0;
    ambiguous_values = 0;
    diag_length = 0u;

    while (index < choice_count) {
        if (strncmp(choices[index].name, value, value_length) == 0) {
            if (choices[index].name[value_length] == '\0') {
                *matched_value = choices[index].value;
                return SIXEL_OPTION_CHOICE_MATCH;
            }
            if (!base_value_set) {
                base_value = choices[index].value;
                base_value_set = 1;
            } else if (choices[index].value != base_value) {
                ambiguous_values = 1;
            }
            if (candidate_index == (-1)) {
                candidate_index = (int)index;
            }
            ++match_count;
            if (diagnostic != NULL && diagnostic_size > 0u) {
                if (diag_length > 0u && diag_length + 2u < diagnostic_size) {
                    diagnostic[diag_length] = ',';
                    diagnostic[diag_length + 1u] = ' ';
                    diag_length += 2u;
                    diagnostic[diag_length] = '\0';
                }
                copy_length = strlen(choices[index].name);
                if (copy_length > diagnostic_size - diag_length - 1u) {
                    copy_length = diagnostic_size - diag_length - 1u;
                }
                memcpy(diagnostic + diag_length,
                       choices[index].name,
                       copy_length);
                diag_length += copy_length;
                diagnostic[diag_length] = '\0';
            }
        }
        ++index;
    }

    if (match_count == 0u) {
        return SIXEL_OPTION_CHOICE_NONE;
    }
    if (!ambiguous_values) {
        *matched_value = choices[candidate_index].value;
        return SIXEL_OPTION_CHOICE_MATCH;
    }

    return SIXEL_OPTION_CHOICE_AMBIGUOUS;
}

static void
sixel_report_ambiguous_prefix(
    char const *option,
    char const *value,
    char const *candidates,
    char *buffer,
    size_t buffer_size)
{
    int written;

    if (buffer == NULL || buffer_size == 0u) {
        return;
    }
    if (candidates != NULL && candidates[0] != '\0') {
        written = snprintf(buffer,
                           buffer_size,
                           "ambiguous prefix \"%s\" for %s (matches: %s).",
                           value,
                           option,
                           candidates);
    } else {
        written = snprintf(buffer,
                           buffer_size,
                           "ambiguous prefix \"%s\" for %s.",
                           value,
                           option);
    }
    (void) written;
    sixel_helper_set_additional_message(buffer);
}

static sixel_option_choice_t const g_option_choices_builtin_palette[] = {
    { "xterm16", SIXEL_BUILTIN_XTERM16 },
    { "xterm256", SIXEL_BUILTIN_XTERM256 },
    { "vt340mono", SIXEL_BUILTIN_VT340_MONO },
    { "vt340color", SIXEL_BUILTIN_VT340_COLOR },
    { "gray1", SIXEL_BUILTIN_G1 },
    { "gray2", SIXEL_BUILTIN_G2 },
    { "gray4", SIXEL_BUILTIN_G4 },
    { "gray8", SIXEL_BUILTIN_G8 }
};

static sixel_option_choice_t const g_option_choices_diffusion[] = {
    { "auto", SIXEL_DIFFUSE_AUTO },
    { "none", SIXEL_DIFFUSE_NONE },
    { "fs", SIXEL_DIFFUSE_FS },
    { "atkinson", SIXEL_DIFFUSE_ATKINSON },
    { "jajuni", SIXEL_DIFFUSE_JAJUNI },
    { "stucki", SIXEL_DIFFUSE_STUCKI },
    { "burkes", SIXEL_DIFFUSE_BURKES },
    { "sierra1", SIXEL_DIFFUSE_SIERRA1 },
    { "sierra2", SIXEL_DIFFUSE_SIERRA2 },
    { "sierra3", SIXEL_DIFFUSE_SIERRA3 },
    { "a_dither", SIXEL_DIFFUSE_A_DITHER },
    { "x_dither", SIXEL_DIFFUSE_X_DITHER },
    { "lso2", SIXEL_DIFFUSE_LSO2 },
};

static sixel_option_choice_t const g_option_choices_diffusion_scan[] = {
    { "auto", SIXEL_SCAN_AUTO },
    { "serpentine", SIXEL_SCAN_SERPENTINE },
    { "raster", SIXEL_SCAN_RASTER }
};

static sixel_option_choice_t const g_option_choices_diffusion_carry[] = {
    { "auto", SIXEL_CARRY_AUTO },
    { "direct", SIXEL_CARRY_DISABLE },
    { "carry", SIXEL_CARRY_ENABLE }
};

static sixel_option_choice_t const g_option_choices_find_largest[] = {
    { "auto", SIXEL_LARGE_AUTO },
    { "norm", SIXEL_LARGE_NORM },
    { "lum", SIXEL_LARGE_LUM }
};

static sixel_option_choice_t const g_option_choices_select_color[] = {
    { "auto", SIXEL_REP_AUTO },
    { "center", SIXEL_REP_CENTER_BOX },
    { "average", SIXEL_REP_AVERAGE_COLORS },
    { "histogram", SIXEL_REP_AVERAGE_PIXELS },
    { "histgram", SIXEL_REP_AVERAGE_PIXELS }
};

static sixel_option_choice_t const g_option_choices_quantize_model[] = {
    { "auto", SIXEL_QUANTIZE_MODEL_AUTO },
    { "heckbert", SIXEL_QUANTIZE_MODEL_MEDIANCUT },
    { "kmeans", SIXEL_QUANTIZE_MODEL_KMEANS }
};

static sixel_option_choice_t const g_option_choices_final_merge[] = {
    { "auto", SIXEL_FINAL_MERGE_AUTO },
    { "none", SIXEL_FINAL_MERGE_NONE },
    { "ward", SIXEL_FINAL_MERGE_WARD },
    { "hkmeans", SIXEL_FINAL_MERGE_HKMEANS }
};

static sixel_option_choice_t const g_option_choices_resampling[] = {
    { "nearest", SIXEL_RES_NEAREST },
    { "gaussian", SIXEL_RES_GAUSSIAN },
    { "hanning", SIXEL_RES_HANNING },
    { "hamming", SIXEL_RES_HAMMING },
    { "bilinear", SIXEL_RES_BILINEAR },
    { "welsh", SIXEL_RES_WELSH },
    { "bicubic", SIXEL_RES_BICUBIC },
    { "lanczos2", SIXEL_RES_LANCZOS2 },
    { "lanczos3", SIXEL_RES_LANCZOS3 },
    { "lanczos4", SIXEL_RES_LANCZOS4 }
};

static sixel_option_choice_t const g_option_choices_quality[] = {
    { "auto", SIXEL_QUALITY_AUTO },
    { "high", SIXEL_QUALITY_HIGH },
    { "low", SIXEL_QUALITY_LOW },
    { "full", SIXEL_QUALITY_FULL }
};

static sixel_option_choice_t const g_option_choices_loopmode[] = {
    { "auto", SIXEL_LOOP_AUTO },
    { "force", SIXEL_LOOP_FORCE },
    { "disable", SIXEL_LOOP_DISABLE }
};

static sixel_option_choice_t const g_option_choices_palette_type[] = {
    { "auto", SIXEL_PALETTETYPE_AUTO },
    { "hls", SIXEL_PALETTETYPE_HLS },
    { "rgb", SIXEL_PALETTETYPE_RGB }
};

static sixel_option_choice_t const g_option_choices_encode_policy[] = {
    { "auto", SIXEL_ENCODEPOLICY_AUTO },
    { "fast", SIXEL_ENCODEPOLICY_FAST },
    { "size", SIXEL_ENCODEPOLICY_SIZE }
};

static sixel_option_choice_t const g_option_choices_lut_policy[] = {
    { "auto", SIXEL_LUT_POLICY_AUTO },
    { "5bit", SIXEL_LUT_POLICY_5BIT },
    { "6bit", SIXEL_LUT_POLICY_6BIT },
    { "robinhood", SIXEL_LUT_POLICY_ROBINHOOD },
    { "hopscotch", SIXEL_LUT_POLICY_HOPSCOTCH },
    { "certlut", SIXEL_LUT_POLICY_CERTLUT }
};

static sixel_option_choice_t const g_option_choices_working_colorspace[] = {
    { "gamma", SIXEL_COLORSPACE_GAMMA },
    { "linear", SIXEL_COLORSPACE_LINEAR },
    { "oklab", SIXEL_COLORSPACE_OKLAB }
};

static sixel_option_choice_t const g_option_choices_output_colorspace[] = {
    { "gamma", SIXEL_COLORSPACE_GAMMA },
    { "linear", SIXEL_COLORSPACE_LINEAR },
    { "smpte-c", SIXEL_COLORSPACE_SMPTEC },
    { "smptec", SIXEL_COLORSPACE_SMPTEC }
};


static char *
arg_strdup(
    char const          /* in */ *s,          /* source buffer */
    sixel_allocator_t   /* in */ *allocator)  /* allocator object for
                                                 destination buffer */
{
    char *p;
    size_t len;

    len = strlen(s);

    p = (char *)sixel_allocator_malloc(allocator, len + 1);
    if (p) {
        (void)sixel_compat_strcpy(p, len + 1, s);
    }
    return p;
}


/* An clone function of XColorSpec() of xlib */
static SIXELSTATUS
sixel_parse_x_colorspec(
    unsigned char       /* out */ **bgcolor,     /* destination buffer */
    char const          /* in */  *s,            /* source buffer */
    sixel_allocator_t   /* in */  *allocator)    /* allocator object for
                                                    destination buffer */
{
    SIXELSTATUS status = SIXEL_FALSE;
    char *p;
    unsigned char components[3];
    int component_index = 0;
    unsigned long v;
    char *endptr;
    char *buf = NULL;
    struct color const *pcolor;

    /* from rgb_lookup.h generated by gpref */
    pcolor = lookup_rgb(s, strlen(s));
    if (pcolor) {
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        (*bgcolor)[0] = pcolor->r;
        (*bgcolor)[1] = pcolor->g;
        (*bgcolor)[2] = pcolor->b;
    } else if (s[0] == 'r' && s[1] == 'g' && s[2] == 'b' && s[3] == ':') {
        p = buf = arg_strdup(s + 4, allocator);
        if (buf == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        while (*p) {
            v = 0;
            for (endptr = p; endptr - p <= 12; ++endptr) {
                if (*endptr >= '0' && *endptr <= '9') {
                    v = (v << 4) | (unsigned long)(*endptr - '0');
                } else if (*endptr >= 'a' && *endptr <= 'f') {
                    v = (v << 4) | (unsigned long)(*endptr - 'a' + 10);
                } else if (*endptr >= 'A' && *endptr <= 'F') {
                    v = (v << 4) | (unsigned long)(*endptr - 'A' + 10);
                } else {
                    break;
                }
            }
            if (endptr - p == 0) {
                break;
            }
            if (endptr - p > 4) {
                break;
            }
            v = v << ((4 - (endptr - p)) * 4) >> 8;
            components[component_index++] = (unsigned char)v;
            p = endptr;
            if (component_index == 3) {
                break;
            }
            if (*p == '\0') {
                break;
            }
            if (*p != '/') {
                break;
            }
            ++p;
        }
        if (component_index != 3 || *p != '\0' || *p == '/') {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        (*bgcolor)[0] = components[0];
        (*bgcolor)[1] = components[1];
        (*bgcolor)[2] = components[2];
    } else if (*s == '#') {
        buf = arg_strdup(s + 1, allocator);
        if (buf == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        for (p = endptr = buf; endptr - p <= 12; ++endptr) {
            if (*endptr >= '0' && *endptr <= '9') {
                *endptr -= '0';
            } else if (*endptr >= 'a' && *endptr <= 'f') {
                *endptr -= 'a' - 10;
            } else if (*endptr >= 'A' && *endptr <= 'F') {
                *endptr -= 'A' - 10;
            } else if (*endptr == '\0') {
                break;
            } else {
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        if (endptr - p > 12) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        *bgcolor = (unsigned char *)sixel_allocator_malloc(allocator, 3);
        if (*bgcolor == NULL) {
            sixel_helper_set_additional_message(
                "sixel_parse_x_colorspec: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        switch (endptr - p) {
        case 3:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4);
            (*bgcolor)[1] = (unsigned char)(p[1] << 4);
            (*bgcolor)[2] = (unsigned char)(p[2] << 4);
            break;
        case 6:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[2] << 4 | p[3]);
            (*bgcolor)[2] = (unsigned char)(p[4] << 4 | p[4]);
            break;
        case 9:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[3] << 4 | p[4]);
            (*bgcolor)[2] = (unsigned char)(p[6] << 4 | p[7]);
            break;
        case 12:
            (*bgcolor)[0] = (unsigned char)(p[0] << 4 | p[1]);
            (*bgcolor)[1] = (unsigned char)(p[4] << 4 | p[5]);
            (*bgcolor)[2] = (unsigned char)(p[8] << 4 | p[9]);
            break;
        default:
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    } else {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    sixel_allocator_free(allocator, buf);

    return status;
}


/* generic writer function for passing to sixel_output_new() */
static int
sixel_write_callback(char *data, int size, void *priv)
{
    int result;

    result = (int)sixel_compat_write(*(int *)priv,
                                     data,
                                     (size_t)size);

    return result;
}


/* the writer function with hex-encoding for passing to sixel_output_new() */
static int
sixel_hex_write_callback(
    char    /* in */ *data,
    int     /* in */ size,
    void    /* in */ *priv)
{
    char hex[SIXEL_OUTPUT_PACKET_SIZE * 2];
    int i;
    int j;
    int result;

    for (i = j = 0; i < size; ++i, ++j) {
        hex[j] = (data[i] >> 4) & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
        hex[++j] = data[i] & 0xf;
        hex[j] += (hex[j] < 10 ? '0': ('a' - 10));
    }

    result = (int)sixel_compat_write(*(int *)priv,
                                     hex,
                                     (size_t)(size * 2));

    return result;
}

typedef struct sixel_encoder_output_probe {
    sixel_encoder_t *encoder;
    sixel_write_function base_write;
    void *base_priv;
} sixel_encoder_output_probe_t;

static int
sixel_write_with_probe(char *data, int size, void *priv)
{
    sixel_encoder_output_probe_t *probe;
    int written;
    double started_at;
    double finished_at;
    double duration;

    probe = (sixel_encoder_output_probe_t *)priv;
    if (probe == NULL || probe->base_write == NULL) {
        return 0;
    }
    started_at = 0.0;
    finished_at = 0.0;
    duration = 0.0;
    if (probe->encoder != NULL &&
            probe->encoder->assessment_observer != NULL) {
        started_at = sixel_assessment_timer_now();
    }
    written = probe->base_write(data, size, probe->base_priv);
    if (probe->encoder != NULL &&
            probe->encoder->assessment_observer != NULL) {
        finished_at = sixel_assessment_timer_now();
        duration = finished_at - started_at;
        if (duration < 0.0) {
            duration = 0.0;
        }
    }
    if (written > 0 && probe->encoder != NULL &&
            probe->encoder->assessment_observer != NULL) {
        sixel_assessment_record_output_write(
            probe->encoder->assessment_observer,
            (size_t)written,
            duration);
    }
    return written;
}

/*
 * Reuse the fn_write probe for raw escape writes so that every
 * assessment bucket receives the same accounting.
 *
 *     encoder        probe wrapper       write(2)
 *     +------+    +----------------+    +---------+
 *     | data | -> | sixel_write_*  | -> | target  |
 *     +------+    +----------------+    +---------+
 */
static int
sixel_encoder_probe_fd_write(sixel_encoder_t *encoder,
                             char *data,
                             int size,
                             int fd)
{
    sixel_encoder_output_probe_t probe;
    int written;

    probe.encoder = encoder;
    probe.base_write = sixel_write_callback;
    probe.base_priv = &fd;
    written = sixel_write_with_probe(data, size, &probe);

    return written;
}

static SIXELSTATUS
sixel_encoder_ensure_cell_size(sixel_encoder_t *encoder)
{
#if defined(TIOCGWINSZ)
    struct winsize ws;
    int result;
    int fd = 0;

    if (encoder->cell_width > 0 && encoder->cell_height > 0) {
        return SIXEL_OK;
    }

    fd = sixel_compat_open("/dev/tty", O_RDONLY);
    if (fd >= 0) {
        result = ioctl(fd, TIOCGWINSZ, &ws);
        (void)sixel_compat_close(fd);
    } else {
        sixel_helper_set_additional_message(
            "failed to open /dev/tty");
        return (SIXEL_LIBC_ERROR | (errno & 0xff));
    }
    if (result != 0) {
        sixel_helper_set_additional_message(
            "failed to query terminal geometry with ioctl().");
        return (SIXEL_LIBC_ERROR | (errno & 0xff));
    }

    if (ws.ws_col <= 0 || ws.ws_row <= 0 ||
        ws.ws_xpixel <= ws.ws_col || ws.ws_ypixel <= ws.ws_row) {
        sixel_helper_set_additional_message(
            "terminal does not report pixel cell size for drcs option.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->cell_width = ws.ws_xpixel / ws.ws_col;
    encoder->cell_height = ws.ws_ypixel / ws.ws_row;
    if (encoder->cell_width <= 0 || encoder->cell_height <= 0) {
        sixel_helper_set_additional_message(
            "terminal cell size reported zero via ioctl().");
        return SIXEL_BAD_ARGUMENT;
    }

    return SIXEL_OK;
#else
    (void) encoder;
    sixel_helper_set_additional_message(
        "drcs option is not supported on this platform.");
    return SIXEL_NOT_IMPLEMENTED;
#endif
}


/* returns monochrome dithering context object */
static SIXELSTATUS
sixel_prepare_monochrome_palette(
    sixel_dither_t  /* out */ **dither,
     int            /* in */  finvert)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (finvert) {
        *dither = sixel_dither_get(SIXEL_BUILTIN_MONO_LIGHT);
    } else {
        *dither = sixel_dither_get(SIXEL_BUILTIN_MONO_DARK);
    }
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_monochrome_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}


/* returns dithering context object with specified builtin palette */
typedef struct palette_conversion {
    unsigned char *original;
    unsigned char *copy;
    size_t size;
    int convert_inplace;
    int converted;
    int frame_colorspace;
} palette_conversion_t;

static SIXELSTATUS
sixel_encoder_convert_palette(sixel_encoder_t *encoder,
                              sixel_output_t *output,
                              sixel_dither_t *dither,
                              int frame_colorspace,
                              int pixelformat,
                              palette_conversion_t *ctx)
{
    SIXELSTATUS status = SIXEL_OK;
    unsigned char *palette;
    int palette_colors;

    ctx->original = NULL;
    ctx->copy = NULL;
    ctx->size = 0;
    ctx->convert_inplace = 0;
    ctx->converted = 0;
    ctx->frame_colorspace = frame_colorspace;

    palette = sixel_dither_get_palette(dither);
    palette_colors = sixel_dither_get_num_of_palette_colors(dither);
    ctx->original = palette;

    if (palette == NULL || palette_colors <= 0 ||
            frame_colorspace == output->colorspace) {
        return SIXEL_OK;
    }

    ctx->size = (size_t)palette_colors * 3;

    output->pixelformat = SIXEL_PIXELFORMAT_RGB888;
    output->source_colorspace = frame_colorspace;

    ctx->copy = (unsigned char *)sixel_allocator_malloc(encoder->allocator,
                                                        ctx->size);
    if (ctx->copy == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_convert_palette: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    memcpy(ctx->copy, palette, ctx->size);

    status = sixel_output_convert_colorspace(output,
                                             palette,
                                             ctx->size);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    ctx->converted = 1;

end:
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;

    return status;
}

static void
sixel_encoder_restore_palette(sixel_encoder_t *encoder,
                              sixel_dither_t *dither,
                              palette_conversion_t *ctx)
{
    if (ctx->copy != NULL && ctx->size > 0) {
        unsigned char *palette;

        palette = sixel_dither_get_palette(dither);
        if (palette != NULL) {
            memcpy(palette, ctx->copy, ctx->size);
        }
        sixel_allocator_free(encoder->allocator, ctx->copy);
        ctx->copy = NULL;
    } else if (ctx->convert_inplace && ctx->converted &&
               ctx->original && ctx->size > 0) {
        (void)sixel_helper_convert_colorspace(ctx->original,
                                              ctx->size,
                                              SIXEL_PIXELFORMAT_RGB888,
                                              SIXEL_COLORSPACE_GAMMA,
                                              ctx->frame_colorspace);
    }
}

static SIXELSTATUS
sixel_encoder_capture_quantized(sixel_encoder_t *encoder,
                                sixel_dither_t *dither,
                                unsigned char const *pixels,
                                size_t size,
                                int width,
                                int height,
                                int pixelformat,
                                int colorspace)
{
    SIXELSTATUS status;
    unsigned char *palette;
    int ncolors;
    size_t palette_bytes;
    unsigned char *new_pixels;
    unsigned char *new_palette;
    size_t capture_bytes;
    unsigned char const *capture_source;
    sixel_index_t *paletted_pixels;
    size_t quantized_pixels;
    sixel_allocator_t *dither_allocator;
    int saved_pixelformat;
    int restore_pixelformat;

    /*
     * Preserve the quantized frame for assessment observers.
     *
     *     +-----------------+     +---------------------+
     *     | quantized bytes | --> | encoder->capture_*  |
     *     +-----------------+     +---------------------+
     */

    status = SIXEL_OK;
    palette = NULL;
    ncolors = 0;
    palette_bytes = 0;
    new_pixels = NULL;
    new_palette = NULL;
    capture_bytes = size;
    capture_source = pixels;
    paletted_pixels = NULL;
    quantized_pixels = 0;
    dither_allocator = NULL;

    if (encoder == NULL || pixels == NULL ||
            (dither == NULL && size == 0)) {
        sixel_helper_set_additional_message(
            "sixel_encoder_capture_quantized: invalid capture request.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (!encoder->capture_quantized) {
        return SIXEL_OK;
    }

    saved_pixelformat = SIXEL_PIXELFORMAT_RGB888;
    restore_pixelformat = 0;
    if (dither != NULL) {
        dither_allocator = dither->allocator;
        saved_pixelformat = dither->pixelformat;
        restore_pixelformat = 1;
        if (width <= 0 || height <= 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_capture_quantized: invalid dimensions.");
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        quantized_pixels = (size_t)width * (size_t)height;
        if (height != 0 &&
                quantized_pixels / (size_t)height != (size_t)width) {
            sixel_helper_set_additional_message(
                "sixel_encoder_capture_quantized: image too large.");
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }
        paletted_pixels = sixel_dither_apply_palette(
            dither, (unsigned char *)pixels, width, height);
        if (paletted_pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_capture_quantized: palette conversion failed.");
            status = SIXEL_RUNTIME_ERROR;
            goto cleanup;
        }
        capture_source = (unsigned char const *)paletted_pixels;
        capture_bytes = quantized_pixels;
    }

    if (capture_bytes > 0) {
        if (encoder->capture_pixels == NULL ||
                encoder->capture_pixels_size < capture_bytes) {
            new_pixels = (unsigned char *)sixel_allocator_malloc(
                encoder->allocator, capture_bytes);
            if (new_pixels == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_capture_quantized: "
                    "sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }
            sixel_allocator_free(encoder->allocator, encoder->capture_pixels);
            encoder->capture_pixels = new_pixels;
            encoder->capture_pixels_size = capture_bytes;
        }
        memcpy(encoder->capture_pixels, capture_source, capture_bytes);
    }
    encoder->capture_pixel_bytes = capture_bytes;

    palette = NULL;
    ncolors = 0;
    palette_bytes = 0;
    if (dither != NULL) {
        palette = sixel_dither_get_palette(dither);
        ncolors = sixel_dither_get_num_of_palette_colors(dither);
    }
    if (palette != NULL && ncolors > 0) {
        palette_bytes = (size_t)ncolors * 3;
        if (encoder->capture_palette == NULL ||
                encoder->capture_palette_size < palette_bytes) {
            new_palette = (unsigned char *)sixel_allocator_malloc(
                encoder->allocator, palette_bytes);
            if (new_palette == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_capture_quantized: "
                    "sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }
            sixel_allocator_free(encoder->allocator,
                                 encoder->capture_palette);
            encoder->capture_palette = new_palette;
            encoder->capture_palette_size = palette_bytes;
        }
        memcpy(encoder->capture_palette, palette, palette_bytes);
    }

    encoder->capture_width = width;
    encoder->capture_height = height;
    if (dither != NULL) {
        encoder->capture_pixelformat = SIXEL_PIXELFORMAT_PAL8;
    } else {
        encoder->capture_pixelformat = pixelformat;
    }
    encoder->capture_colorspace = colorspace;
    encoder->capture_palette_size = palette_bytes;
    encoder->capture_ncolors = ncolors;
    encoder->capture_valid = 1;

cleanup:
    if (restore_pixelformat && dither != NULL) {
        /*
         * Undo the normalization performed by sixel_dither_apply_palette().
         *
         *     RGBA8888 --capture--> RGB888 (temporary)
         *          \______________________________/
         *                          |
         *                 restore original state for
         *                 the real encoder execution.
         */
        sixel_dither_set_pixelformat(dither, saved_pixelformat);
    }
    if (paletted_pixels != NULL && dither_allocator != NULL) {
        sixel_allocator_free(dither_allocator, paletted_pixels);
    }

    return status;
}

static SIXELSTATUS
sixel_prepare_builtin_palette(
    sixel_dither_t /* out */ **dither,
    int            /* in */  builtin_palette)
{
    SIXELSTATUS status = SIXEL_FALSE;

    *dither = sixel_dither_get(builtin_palette);
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_builtin_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}

static int
sixel_encoder_thumbnail_hint(sixel_encoder_t *encoder)
{
    int width_hint;
    int height_hint;
    long base;
    long size;

    width_hint = 0;
    height_hint = 0;
    base = 0;
    size = 0;

    if (encoder == NULL) {
        return 0;
    }

    width_hint = encoder->pixelwidth;
    height_hint = encoder->pixelheight;

    /* Request extra resolution for downscaling to preserve detail. */
    if (width_hint > 0 && height_hint > 0) {
        /* Follow the CLI rule: double the larger axis before doubling
         * again for the final request size. */
        if (width_hint >= height_hint) {
            base = (long)width_hint;
        } else {
            base = (long)height_hint;
        }
        base *= 2L;
    } else if (width_hint > 0) {
        base = (long)width_hint;
    } else if (height_hint > 0) {
        base = (long)height_hint;
    } else {
        return 0;
    }

    size = base * 2L;
    if (size > (long)INT_MAX) {
        size = (long)INT_MAX;
    }
    if (size < 1L) {
        size = 1L;
    }

    return (int)size;
}


typedef struct sixel_callback_context_for_mapfile {
    int reqcolors;
    sixel_dither_t *dither;
    sixel_allocator_t *allocator;
    int working_colorspace;
    int lut_policy;
} sixel_callback_context_for_mapfile_t;


/* callback function for sixel_helper_load_image_file() */
static SIXELSTATUS
load_image_callback_for_palette(
    sixel_frame_t   /* in */    *frame, /* frame object from image loader */
    void            /* in */    *data)  /* private data */
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_callback_context_for_mapfile_t *callback_context;

    /* get callback context object from the private data */
    callback_context = (sixel_callback_context_for_mapfile_t *)data;

    status = sixel_frame_ensure_colorspace(frame,
                                           callback_context->working_colorspace);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    switch (sixel_frame_get_pixelformat(frame)) {
    case SIXEL_PIXELFORMAT_PAL1:
    case SIXEL_PIXELFORMAT_PAL2:
    case SIXEL_PIXELFORMAT_PAL4:
    case SIXEL_PIXELFORMAT_PAL8:
        if (sixel_frame_get_palette(frame) == NULL) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        /* create new dither object */
        status = sixel_dither_new(
            &callback_context->dither,
            sixel_frame_get_ncolors(frame),
            callback_context->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        sixel_dither_set_lut_policy(callback_context->dither,
                                    callback_context->lut_policy);

        /* use palette which is extracted from the image */
        sixel_dither_set_palette(callback_context->dither,
                                 sixel_frame_get_palette(frame));
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G1:
        /* use 1bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G1);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G2:
        /* use 2bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G1);
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G2);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G4:
        /* use 4bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G4);
        /* success */
        status = SIXEL_OK;
        break;
    case SIXEL_PIXELFORMAT_G8:
        /* use 8bpp grayscale builtin palette */
        callback_context->dither = sixel_dither_get(SIXEL_BUILTIN_G8);
        /* success */
        status = SIXEL_OK;
        break;
    default:
        /* create new dither object */
        status = sixel_dither_new(
            &callback_context->dither,
            callback_context->reqcolors,
            callback_context->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        sixel_dither_set_lut_policy(callback_context->dither,
                                    callback_context->lut_policy);

        /* create adaptive palette from given frame object */
        status = sixel_dither_initialize(callback_context->dither,
                                         sixel_frame_get_pixels(frame),
                                         sixel_frame_get_width(frame),
                                         sixel_frame_get_height(frame),
                                         sixel_frame_get_pixelformat(frame),
                                         SIXEL_LARGE_NORM,
                                         SIXEL_REP_CENTER_BOX,
                                         SIXEL_QUALITY_HIGH);
        if (SIXEL_FAILED(status)) {
            sixel_dither_unref(callback_context->dither);
            goto end;
        }

        /* success */
        status = SIXEL_OK;

        break;
    }

end:
    return status;
}


static SIXELSTATUS
sixel_encoder_emit_palette_output(sixel_encoder_t *encoder);


static int
sixel_path_has_extension(char const *path, char const *extension)
{
    size_t path_len;
    size_t ext_len;
    size_t index;

    path_len = 0u;
    ext_len = 0u;
    index = 0u;

    if (path == NULL || extension == NULL) {
        return 0;
    }

    path_len = strlen(path);
    ext_len = strlen(extension);
    if (ext_len == 0u || path_len < ext_len) {
        return 0;
    }

    for (index = 0u; index < ext_len; ++index) {
        unsigned char path_ch;
        unsigned char ext_ch;

        path_ch = (unsigned char)path[path_len - ext_len + index];
        ext_ch = (unsigned char)extension[index];
        if (tolower(path_ch) != tolower(ext_ch)) {
            return 0;
        }
    }

    return 1;
}

typedef enum sixel_palette_format {
    SIXEL_PALETTE_FORMAT_NONE = 0,
    SIXEL_PALETTE_FORMAT_ACT,
    SIXEL_PALETTE_FORMAT_PAL_JASC,
    SIXEL_PALETTE_FORMAT_PAL_RIFF,
    SIXEL_PALETTE_FORMAT_PAL_AUTO,
    SIXEL_PALETTE_FORMAT_GPL
} sixel_palette_format_t;

/*
 * Palette specification parser
 *
 *   TYPE:PATH  -> explicit format prefix
 *   PATH       -> rely on extension or heuristics
 *
 * The ASCII diagram below shows how the prefix is peeled:
 *
 *   [type] : [path]
 *    ^-- left part selects decoder/encoder when present.
 */
static char const *
sixel_palette_strip_prefix(char const *spec,
                           sixel_palette_format_t *format_hint)
{
    char const *colon;
    size_t type_len;
    size_t index;
    char lowered[16];

    colon = NULL;
    type_len = 0u;
    index = 0u;

    if (format_hint != NULL) {
        *format_hint = SIXEL_PALETTE_FORMAT_NONE;
    }
    if (spec == NULL) {
        return NULL;
    }

    colon = strchr(spec, ':');
    if (colon == NULL) {
        return spec;
    }

    type_len = (size_t)(colon - spec);
    if (type_len == 0u || type_len >= sizeof(lowered)) {
        return spec;
    }

    for (index = 0u; index < type_len; ++index) {
        lowered[index] = (char)tolower((unsigned char)spec[index]);
    }
    lowered[type_len] = '\0';

    if (strcmp(lowered, "act") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_ACT;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_AUTO;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal-jasc") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_JASC;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "pal-riff") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_PAL_RIFF;
        }
        return colon + 1;
    }
    if (strcmp(lowered, "gpl") == 0) {
        if (format_hint != NULL) {
            *format_hint = SIXEL_PALETTE_FORMAT_GPL;
        }
        return colon + 1;
    }

    return spec;
}

static sixel_palette_format_t
sixel_palette_format_from_extension(char const *path)
{
    if (path == NULL) {
        return SIXEL_PALETTE_FORMAT_NONE;
    }

    if (sixel_path_has_extension(path, ".act")) {
        return SIXEL_PALETTE_FORMAT_ACT;
    }
    if (sixel_path_has_extension(path, ".pal")) {
        return SIXEL_PALETTE_FORMAT_PAL_AUTO;
    }
    if (sixel_path_has_extension(path, ".gpl")) {
        return SIXEL_PALETTE_FORMAT_GPL;
    }

    return SIXEL_PALETTE_FORMAT_NONE;
}

static int
sixel_path_has_any_extension(char const *path)
{
    char const *slash_forward;
#if defined(_WIN32)
    char const *slash_backward;
#endif
    char const *start;
    char const *dot;

    slash_forward = NULL;
#if defined(_WIN32)
    slash_backward = NULL;
#endif
    start = path;
    dot = NULL;

    if (path == NULL) {
        return 0;
    }

    slash_forward = strrchr(path, '/');
#if defined(_WIN32)
    slash_backward = strrchr(path, '\\');
    if (slash_backward != NULL &&
            (slash_forward == NULL || slash_backward > slash_forward)) {
        slash_forward = slash_backward;
    }
#endif
    if (slash_forward == NULL) {
        start = path;
    } else {
        start = slash_forward + 1;
    }

    dot = strrchr(start, '.');
    if (dot == NULL) {
        return 0;
    }

    if (dot[1] == '\0') {
        return 0;
    }

    return 1;
}

static int
sixel_palette_has_utf8_bom(unsigned char const *data, size_t size)
{
    if (data == NULL || size < 3u) {
        return 0;
    }
    if (data[0] == 0xefu && data[1] == 0xbbu && data[2] == 0xbfu) {
        return 1;
    }
    return 0;
}


/*
 * Materialize palette bytes from a stream.
 *
 * The flow looks like:
 *
 *   stream --> [scratch buffer] --> [resizable heap buffer]
 *                  ^ looped read        ^ returned payload
 */
static SIXELSTATUS
sixel_palette_read_stream(FILE *stream,
                          sixel_allocator_t *allocator,
                          unsigned char **pdata,
                          size_t *psize)
{
    SIXELSTATUS status;
    unsigned char *buffer;
    unsigned char *grown;
    size_t capacity;
    size_t used;
    size_t read_bytes;
    size_t needed;
    size_t new_capacity;
    unsigned char scratch[4096];

    status = SIXEL_FALSE;
    buffer = NULL;
    grown = NULL;
    capacity = 0u;
    used = 0u;
    read_bytes = 0u;
    needed = 0u;
    new_capacity = 0u;

    if (pdata == NULL || psize == NULL || stream == NULL || allocator == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_read_stream: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    *pdata = NULL;
    *psize = 0u;

    while (1) {
        read_bytes = fread(scratch, 1, sizeof(scratch), stream);
        if (read_bytes == 0u) {
            if (ferror(stream)) {
                sixel_helper_set_additional_message(
                    "sixel_palette_read_stream: fread() failed.");
                status = SIXEL_LIBC_ERROR;
                goto cleanup;
            }
            break;
        }

        if (used > SIZE_MAX - read_bytes) {
            sixel_helper_set_additional_message(
                "sixel_palette_read_stream: size overflow.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        needed = used + read_bytes;

        if (needed > capacity) {
            new_capacity = capacity;
            if (new_capacity == 0u) {
                new_capacity = 4096u;
            }
            while (needed > new_capacity) {
                if (new_capacity > SIZE_MAX / 2u) {
                    sixel_helper_set_additional_message(
                        "sixel_palette_read_stream: size overflow.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto cleanup;
                }
                new_capacity *= 2u;
            }

            grown = (unsigned char *)sixel_allocator_malloc(allocator,
                                                             new_capacity);
            if (grown == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_palette_read_stream: allocation failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto cleanup;
            }

            if (buffer != NULL) {
                memcpy(grown, buffer, used);
                sixel_allocator_free(allocator, buffer);
            }

            buffer = grown;
            grown = NULL;
            capacity = new_capacity;
        }

        memcpy(buffer + used, scratch, read_bytes);
        used += read_bytes;
    }

    *pdata = buffer;
    *psize = used;
    status = SIXEL_OK;
    return status;

cleanup:
    if (grown != NULL) {
        sixel_allocator_free(allocator, grown);
    }
    if (buffer != NULL) {
        sixel_allocator_free(allocator, buffer);
    }
    return status;
}


static SIXELSTATUS
sixel_palette_open_read(char const *path, FILE **pstream, int *pclose)
{
    if (pstream == NULL || pclose == NULL || path == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_open_read: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (strcmp(path, "-") == 0) {
        *pstream = stdin;
        *pclose = 0;
        return SIXEL_OK;
    }

    *pstream = fopen(path, "rb");
    if (*pstream == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_open_read: failed to open file.");
        return SIXEL_LIBC_ERROR;
    }

    *pclose = 1;
    return SIXEL_OK;
}


static void
sixel_palette_close_stream(FILE *stream, int close_stream)
{
    if (close_stream && stream != NULL) {
        (void) fclose(stream);
    }
}


static sixel_palette_format_t
sixel_palette_guess_format(unsigned char const *data, size_t size)
{
    size_t offset;
    size_t data_size;

    offset = 0u;
    data_size = size;

    if (data == NULL || size == 0u) {
        return SIXEL_PALETTE_FORMAT_NONE;
    }

    if (size == 256u * 3u || size == 256u * 3u + 4u) {
        return SIXEL_PALETTE_FORMAT_ACT;
    }

    if (size >= 12u && memcmp(data, "RIFF", 4) == 0
            && memcmp(data + 8, "PAL ", 4) == 0) {
        return SIXEL_PALETTE_FORMAT_PAL_RIFF;
    }

    if (sixel_palette_has_utf8_bom(data, size)) {
        offset = 3u;
        data_size = size - 3u;
    }

    if (data_size >= 8u && memcmp(data + offset, "JASC-PAL", 8) == 0) {
        return SIXEL_PALETTE_FORMAT_PAL_JASC;
    }
    if (data_size >= 12u && memcmp(data + offset, "GIMP Palette", 12) == 0) {
        return SIXEL_PALETTE_FORMAT_GPL;
    }

    return SIXEL_PALETTE_FORMAT_NONE;
}


static unsigned int
sixel_palette_read_le16(unsigned char const *ptr)
{
    if (ptr == NULL) {
        return 0u;
    }
    return (unsigned int)ptr[0] | ((unsigned int)ptr[1] << 8);
}


static unsigned int
sixel_palette_read_le32(unsigned char const *ptr)
{
    if (ptr == NULL) {
        return 0u;
    }
    return ((unsigned int)ptr[0])
        | ((unsigned int)ptr[1] << 8)
        | ((unsigned int)ptr[2] << 16)
        | ((unsigned int)ptr[3] << 24);
}


/*
 * Adobe Color Table (*.act) reader
 *
 *   +-----------+---------------------------+
 *   | section   | bytes                     |
 *   +-----------+---------------------------+
 *   | palette   | 256 entries * 3 RGB bytes |
 *   | trailer   | optional count/start pair |
 *   +-----------+---------------------------+
 */
static SIXELSTATUS
sixel_palette_parse_act(unsigned char const *data,
                        size_t size,
                        sixel_encoder_t *encoder,
                        sixel_dither_t **dither)
{
    SIXELSTATUS status;
    sixel_dither_t *local;
    unsigned char const *palette_start;
    unsigned char const *trailer;
    unsigned char *target;
    size_t copy_bytes;
    int exported_colors;
    int start_index;

    status = SIXEL_FALSE;
    local = NULL;
    palette_start = data;
    trailer = NULL;
    target = NULL;
    copy_bytes = 0u;
    exported_colors = 0;
    start_index = 0;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size < 256u * 3u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: truncated ACT palette.");
        return SIXEL_BAD_INPUT;
    }

    if (size == 256u * 3u) {
        exported_colors = 256;
        start_index = 0;
    } else if (size == 256u * 3u + 4u) {
        trailer = data + 256u * 3u;
        exported_colors = (int)(((unsigned int)trailer[0] << 8)
                                | (unsigned int)trailer[1]);
        start_index = (int)(((unsigned int)trailer[2] << 8)
                            | (unsigned int)trailer[3]);
    } else {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: invalid ACT length.");
        return SIXEL_BAD_INPUT;
    }

    if (start_index < 0 || start_index >= 256) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: ACT start index out of range.");
        return SIXEL_BAD_INPUT;
    }
    if (exported_colors <= 0 || exported_colors > 256) {
        exported_colors = 256;
    }
    if (start_index + exported_colors > 256) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_act: ACT palette exceeds 256 slots.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_dither_new(&local, exported_colors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    sixel_dither_set_lut_policy(local, encoder->lut_policy);

    target = sixel_dither_get_palette(local);
    copy_bytes = (size_t)exported_colors * 3u;
    memcpy(target, palette_start + (size_t)start_index * 3u, copy_bytes);

    *dither = local;
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_parse_pal_jasc(unsigned char const *data,
                             size_t size,
                             sixel_encoder_t *encoder,
                             sixel_dither_t **dither)
{
    SIXELSTATUS status;
    char *text;
    size_t index;
    size_t offset;
    char *cursor;
    char *line;
    char *line_end;
    int stage;
    int exported_colors;
    int parsed_colors;
    sixel_dither_t *local;
    unsigned char *target;
    long component;
    char *parse_end;
    int value_index;
    int values[3];
    char tail;

    status = SIXEL_FALSE;
    text = NULL;
    index = 0u;
    offset = 0u;
    cursor = NULL;
    line = NULL;
    line_end = NULL;
    stage = 0;
    exported_colors = 0;
    parsed_colors = 0;
    local = NULL;
    target = NULL;
    component = 0;
    parse_end = NULL;
    value_index = 0;
    values[0] = 0;
    values[1] = 0;
    values[2] = 0;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size == 0u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: empty palette.");
        return SIXEL_BAD_INPUT;
    }

    text = (char *)sixel_allocator_malloc(encoder->allocator, size + 1u);
    if (text == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(text, data, size);
    text[size] = '\0';

    if (sixel_palette_has_utf8_bom((unsigned char const *)text, size)) {
        offset = 3u;
    }
    cursor = text + offset;

    while (*cursor != '\0') {
        line = cursor;
        line_end = cursor;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            ++line_end;
        }
        if (*line_end != '\0') {
            *line_end = '\0';
            cursor = line_end + 1;
        } else {
            cursor = line_end;
        }
        while (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }

        while (*line == ' ' || *line == '\t') {
            ++line;
        }
        index = strlen(line);
        while (index > 0u) {
            tail = line[index - 1];
            if (tail != ' ' && tail != '\t') {
                break;
            }
            line[index - 1] = '\0';
            --index;
        }
        if (*line == '\0') {
            continue;
        }
        if (*line == '#') {
            continue;
        }

        if (stage == 0) {
            if (strcmp(line, "JASC-PAL") != 0) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: missing header.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            stage = 1;
            continue;
        }
        if (stage == 1) {
            stage = 2;
            continue;
        }
        if (stage == 2) {
            component = strtol(line, &parse_end, 10);
            if (parse_end == line || component <= 0L || component > 256L) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: invalid color count.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            exported_colors = (int)component;
            status = sixel_dither_new(&local, exported_colors,
                                      encoder->allocator);
            if (SIXEL_FAILED(status)) {
                goto cleanup;
            }
            sixel_dither_set_lut_policy(local, encoder->lut_policy);
            target = sixel_dither_get_palette(local);
            stage = 3;
            continue;
        }

        value_index = 0;
        while (value_index < 3) {
            component = strtol(line, &parse_end, 10);
            if (parse_end == line || component < 0L || component > 255L) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_pal_jasc: invalid component.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            values[value_index] = (int)component;
            ++value_index;
            line = parse_end;
            while (*line == ' ' || *line == '\t') {
                ++line;
            }
        }

        if (parsed_colors >= exported_colors) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_jasc: excess entries.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        target[parsed_colors * 3 + 0] =
            (unsigned char)values[0];
        target[parsed_colors * 3 + 1] =
            (unsigned char)values[1];
        target[parsed_colors * 3 + 2] =
            (unsigned char)values[2];
        ++parsed_colors;
    }

    if (stage < 3) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: incomplete header.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (parsed_colors != exported_colors) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_jasc: color count mismatch.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    *dither = local;
    status = SIXEL_OK;

cleanup:
    if (SIXEL_FAILED(status) && local != NULL) {
        sixel_dither_unref(local);
    }
    if (text != NULL) {
        sixel_allocator_free(encoder->allocator, text);
    }
    return status;
}


static SIXELSTATUS
sixel_palette_parse_pal_riff(unsigned char const *data,
                             size_t size,
                             sixel_encoder_t *encoder,
                             sixel_dither_t **dither)
{
    SIXELSTATUS status;
    size_t offset;
    size_t chunk_size;
    sixel_dither_t *local;
    unsigned char const *chunk;
    unsigned char *target;
    unsigned int entry_count;
    unsigned int version;
    unsigned int index;
    size_t palette_offset;

    status = SIXEL_FALSE;
    offset = 0u;
    chunk_size = 0u;
    local = NULL;
    chunk = NULL;
    target = NULL;
    entry_count = 0u;
    version = 0u;
    index = 0u;
    palette_offset = 0u;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size < 12u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: truncated palette.");
        return SIXEL_BAD_INPUT;
    }
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "PAL ", 4) != 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: missing RIFF header.");
        return SIXEL_BAD_INPUT;
    }

    offset = 12u;
    while (offset + 8u <= size) {
        chunk = data + offset;
        chunk_size = (size_t)sixel_palette_read_le32(chunk + 4);
        if (offset + 8u + chunk_size > size) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_pal_riff: chunk extends past end.");
            return SIXEL_BAD_INPUT;
        }
        if (memcmp(chunk, "data", 4) == 0) {
            break;
        }
        offset += 8u + ((chunk_size + 1u) & ~1u);
    }

    if (offset + 8u > size || memcmp(chunk, "data", 4) != 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: missing data chunk.");
        return SIXEL_BAD_INPUT;
    }

    if (chunk_size < 4u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: data chunk too small.");
        return SIXEL_BAD_INPUT;
    }
    version = sixel_palette_read_le16(chunk + 8);
    (void)version;
    entry_count = sixel_palette_read_le16(chunk + 10);
    if (entry_count == 0u || entry_count > 256u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: invalid entry count.");
        return SIXEL_BAD_INPUT;
    }
    if (chunk_size != 4u + (size_t)entry_count * 4u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_pal_riff: unexpected chunk size.");
        return SIXEL_BAD_INPUT;
    }

    status = sixel_dither_new(&local, (int)entry_count, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    sixel_dither_set_lut_policy(local, encoder->lut_policy);
    target = sixel_dither_get_palette(local);
    palette_offset = 12u;
    for (index = 0u; index < entry_count; ++index) {
        target[index * 3u + 0u] =
            chunk[palette_offset + index * 4u + 0u];
        target[index * 3u + 1u] =
            chunk[palette_offset + index * 4u + 1u];
        target[index * 3u + 2u] =
            chunk[palette_offset + index * 4u + 2u];
    }

    *dither = local;
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_parse_gpl(unsigned char const *data,
                        size_t size,
                        sixel_encoder_t *encoder,
                        sixel_dither_t **dither)
{
    SIXELSTATUS status;
    char *text;
    size_t offset;
    char *cursor;
    char *line;
    char *line_end;
    size_t index;
    int header_seen;
    int parsed_colors;
    unsigned char palette_bytes[256 * 3];
    long component;
    char *parse_end;
    int value_index;
    int values[3];
    sixel_dither_t *local;
    unsigned char *target;
    char tail;

    status = SIXEL_FALSE;
    text = NULL;
    offset = 0u;
    cursor = NULL;
    line = NULL;
    line_end = NULL;
    index = 0u;
    header_seen = 0;
    parsed_colors = 0;
    component = 0;
    parse_end = NULL;
    value_index = 0;
    values[0] = 0;
    values[1] = 0;
    values[2] = 0;
    local = NULL;
    target = NULL;

    if (encoder == NULL || dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }
    if (data == NULL || size == 0u) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: empty palette.");
        return SIXEL_BAD_INPUT;
    }

    text = (char *)sixel_allocator_malloc(encoder->allocator, size + 1u);
    if (text == NULL) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: allocation failed.");
        return SIXEL_BAD_ALLOCATION;
    }
    memcpy(text, data, size);
    text[size] = '\0';

    if (sixel_palette_has_utf8_bom((unsigned char const *)text, size)) {
        offset = 3u;
    }
    cursor = text + offset;

    while (*cursor != '\0') {
        line = cursor;
        line_end = cursor;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
            ++line_end;
        }
        if (*line_end != '\0') {
            *line_end = '\0';
            cursor = line_end + 1;
        } else {
            cursor = line_end;
        }
        while (*cursor == '\n' || *cursor == '\r') {
            ++cursor;
        }

        while (*line == ' ' || *line == '\t') {
            ++line;
        }
        index = strlen(line);
        while (index > 0u) {
            tail = line[index - 1];
            if (tail != ' ' && tail != '\t') {
                break;
            }
            line[index - 1] = '\0';
            --index;
        }
        if (*line == '\0') {
            continue;
        }
        if (*line == '#') {
            continue;
        }
        if (strncmp(line, "Name:", 5) == 0) {
            continue;
        }
        if (strncmp(line, "Columns:", 8) == 0) {
            continue;
        }

        if (!header_seen) {
            if (strcmp(line, "GIMP Palette") != 0) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_gpl: missing header.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            header_seen = 1;
            continue;
        }

        if (parsed_colors >= 256) {
            sixel_helper_set_additional_message(
                "sixel_palette_parse_gpl: too many colors.");
            status = SIXEL_BAD_INPUT;
            goto cleanup;
        }

        value_index = 0;
        while (value_index < 3) {
            component = strtol(line, &parse_end, 10);
            if (parse_end == line || component < 0L || component > 255L) {
                sixel_helper_set_additional_message(
                    "sixel_palette_parse_gpl: invalid component.");
                status = SIXEL_BAD_INPUT;
                goto cleanup;
            }
            values[value_index] = (int)component;
            ++value_index;
            line = parse_end;
            while (*line == ' ' || *line == '\t') {
                ++line;
            }
        }

        palette_bytes[parsed_colors * 3 + 0] =
            (unsigned char)values[0];
        palette_bytes[parsed_colors * 3 + 1] =
            (unsigned char)values[1];
        palette_bytes[parsed_colors * 3 + 2] =
            (unsigned char)values[2];
        ++parsed_colors;
    }

    if (!header_seen) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: header missing.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (parsed_colors <= 0) {
        sixel_helper_set_additional_message(
            "sixel_palette_parse_gpl: no colors parsed.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }

    status = sixel_dither_new(&local, parsed_colors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    sixel_dither_set_lut_policy(local, encoder->lut_policy);
    target = sixel_dither_get_palette(local);
    memcpy(target, palette_bytes, (size_t)parsed_colors * 3u);

    *dither = local;
    status = SIXEL_OK;

cleanup:
    if (SIXEL_FAILED(status) && local != NULL) {
        sixel_dither_unref(local);
    }
    if (text != NULL) {
        sixel_allocator_free(encoder->allocator, text);
    }
    return status;
}


/*
 * Palette exporters
 *
 *   +----------+-------------------------+
 *   | format   | emission strategy       |
 *   +----------+-------------------------+
 *   | ACT      | fixed 256 entries + EOF |
 *   | PAL JASC | textual lines           |
 *   | PAL RIFF | RIFF container          |
 *   | GPL      | textual lines           |
 *   +----------+-------------------------+
 */
static SIXELSTATUS
sixel_palette_write_act(FILE *stream,
                        unsigned char const *palette,
                        int exported_colors)
{
    SIXELSTATUS status;
    unsigned char act_table[256 * 3];
    unsigned char trailer[4];
    size_t exported_bytes;

    status = SIXEL_FALSE;
    exported_bytes = 0u;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (exported_colors > 256) {
        exported_colors = 256;
    }

    memset(act_table, 0, sizeof(act_table));
    exported_bytes = (size_t)exported_colors * 3u;
    memcpy(act_table, palette, exported_bytes);

    trailer[0] = (unsigned char)(((unsigned int)exported_colors >> 8)
                                 & 0xffu);
    trailer[1] = (unsigned char)((unsigned int)exported_colors & 0xffu);
    trailer[2] = 0u;
    trailer[3] = 0u;

    if (fwrite(act_table, 1, sizeof(act_table), stream)
            != sizeof(act_table)) {
        status = SIXEL_LIBC_ERROR;
        return status;
    }
    if (fwrite(trailer, 1, sizeof(trailer), stream)
            != sizeof(trailer)) {
        status = SIXEL_LIBC_ERROR;
        return status;
    }

    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_write_pal_jasc(FILE *stream,
                             unsigned char const *palette,
                             int exported_colors)
{
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (fprintf(stream, "JASC-PAL\n0100\n%d\n", exported_colors) < 0) {
        return SIXEL_LIBC_ERROR;
    }
    for (index = 0; index < exported_colors; ++index) {
        if (fprintf(stream, "%d %d %d\n",
                    (int)palette[index * 3 + 0],
                    (int)palette[index * 3 + 1],
                    (int)palette[index * 3 + 2]) < 0) {
            return SIXEL_LIBC_ERROR;
        }
    }
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_write_pal_riff(FILE *stream,
                             unsigned char const *palette,
                             int exported_colors)
{
    unsigned char header[12];
    unsigned char chunk[8];
    unsigned char log_palette[4 + 256 * 4];
    unsigned int data_size;
    unsigned int riff_size;
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (exported_colors > 256) {
        exported_colors = 256;
    }

    data_size = 4u + (unsigned int)exported_colors * 4u;
    riff_size = 4u + 8u + data_size;

    memcpy(header, "RIFF", 4);
    header[4] = (unsigned char)(riff_size & 0xffu);
    header[5] = (unsigned char)((riff_size >> 8) & 0xffu);
    header[6] = (unsigned char)((riff_size >> 16) & 0xffu);
    header[7] = (unsigned char)((riff_size >> 24) & 0xffu);
    memcpy(header + 8, "PAL ", 4);

    memcpy(chunk, "data", 4);
    chunk[4] = (unsigned char)(data_size & 0xffu);
    chunk[5] = (unsigned char)((data_size >> 8) & 0xffu);
    chunk[6] = (unsigned char)((data_size >> 16) & 0xffu);
    chunk[7] = (unsigned char)((data_size >> 24) & 0xffu);

    memset(log_palette, 0, sizeof(log_palette));
    log_palette[0] = 0x00;
    log_palette[1] = 0x03;
    log_palette[2] = (unsigned char)(exported_colors & 0xff);
    log_palette[3] = (unsigned char)((exported_colors >> 8) & 0xff);
    for (index = 0; index < exported_colors; ++index) {
        log_palette[4 + index * 4 + 0] = palette[index * 3 + 0];
        log_palette[4 + index * 4 + 1] = palette[index * 3 + 1];
        log_palette[4 + index * 4 + 2] = palette[index * 3 + 2];
        log_palette[4 + index * 4 + 3] = 0u;
    }

    if (fwrite(header, 1, sizeof(header), stream)
            != sizeof(header)) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite(chunk, 1, sizeof(chunk), stream) != sizeof(chunk)) {
        return SIXEL_LIBC_ERROR;
    }
    if (fwrite(log_palette, 1, (size_t)data_size, stream)
            != (size_t)data_size) {
        return SIXEL_LIBC_ERROR;
    }
    return SIXEL_OK;
}


static SIXELSTATUS
sixel_palette_write_gpl(FILE *stream,
                        unsigned char const *palette,
                        int exported_colors)
{
    int index;

    if (stream == NULL || palette == NULL || exported_colors <= 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (fprintf(stream, "GIMP Palette\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "Name: libsixel export\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "Columns: 16\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    if (fprintf(stream, "# Exported by libsixel\n") < 0) {
        return SIXEL_LIBC_ERROR;
    }
    for (index = 0; index < exported_colors; ++index) {
        if (fprintf(stream, "%3d %3d %3d\tIndex %d\n",
                    (int)palette[index * 3 + 0],
                    (int)palette[index * 3 + 1],
                    (int)palette[index * 3 + 2],
                    index) < 0) {
            return SIXEL_LIBC_ERROR;
        }
    }
    return SIXEL_OK;
}


/* create palette from specified map file */
static SIXELSTATUS
sixel_prepare_specified_palette(
    sixel_dither_t  /* out */   **dither,
    sixel_encoder_t /* in */    *encoder)
{
    SIXELSTATUS status;
    sixel_callback_context_for_mapfile_t callback_context;
    sixel_loader_t *loader;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    int loop_override;
    char const *path;
    sixel_palette_format_t format_hint;
    sixel_palette_format_t format_ext;
    sixel_palette_format_t format_final;
    sixel_palette_format_t format_detected;
    FILE *stream;
    int close_stream;
    unsigned char *buffer;
    size_t buffer_size;
    int palette_request;
    int need_detection;
    int treat_as_image;
    int path_has_extension;

    status = SIXEL_FALSE;
    loader = NULL;
    fstatic = 1;
    fuse_palette = 1;
    reqcolors = SIXEL_PALETTE_MAX;
    loop_override = SIXEL_LOOP_DISABLE;
    path = NULL;
    format_hint = SIXEL_PALETTE_FORMAT_NONE;
    format_ext = SIXEL_PALETTE_FORMAT_NONE;
    format_final = SIXEL_PALETTE_FORMAT_NONE;
    format_detected = SIXEL_PALETTE_FORMAT_NONE;
    stream = NULL;
    close_stream = 0;
    buffer = NULL;
    buffer_size = 0u;
    palette_request = 0;
    need_detection = 0;
    treat_as_image = 0;
    path_has_extension = 0;

    if (dither == NULL || encoder == NULL || encoder->mapfile == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_specified_palette: invalid mapfile path.");
        return SIXEL_BAD_ARGUMENT;
    }

    path = sixel_palette_strip_prefix(encoder->mapfile, &format_hint);
    if (path == NULL || *path == '\0') {
        sixel_helper_set_additional_message(
            "sixel_prepare_specified_palette: empty mapfile path.");
        return SIXEL_BAD_ARGUMENT;
    }

    format_ext = sixel_palette_format_from_extension(path);
    path_has_extension = sixel_path_has_any_extension(path);

    if (format_hint != SIXEL_PALETTE_FORMAT_NONE) {
        palette_request = 1;
        format_final = format_hint;
    } else if (format_ext != SIXEL_PALETTE_FORMAT_NONE) {
        palette_request = 1;
        format_final = format_ext;
    } else if (!path_has_extension) {
        palette_request = 1;
        need_detection = 1;
    } else {
        treat_as_image = 1;
    }

    if (palette_request) {
        status = sixel_palette_open_read(path, &stream, &close_stream);
        if (SIXEL_FAILED(status)) {
            goto palette_cleanup;
        }
        status = sixel_palette_read_stream(stream,
                                           encoder->allocator,
                                           &buffer,
                                           &buffer_size);
        if (close_stream) {
            sixel_palette_close_stream(stream, close_stream);
            stream = NULL;
            close_stream = 0;
        }
        if (SIXEL_FAILED(status)) {
            goto palette_cleanup;
        }
        if (buffer_size == 0u) {
            sixel_helper_set_additional_message(
                "sixel_prepare_specified_palette: mapfile is empty.");
            status = SIXEL_BAD_INPUT;
            goto palette_cleanup;
        }

        if (format_final == SIXEL_PALETTE_FORMAT_NONE) {
            format_detected = sixel_palette_guess_format(buffer,
                                                         buffer_size);
            if (format_detected == SIXEL_PALETTE_FORMAT_NONE) {
                sixel_helper_set_additional_message(
                    "sixel_prepare_specified_palette: "
                    "unable to detect palette format.");
                status = SIXEL_BAD_INPUT;
                goto palette_cleanup;
            }
            format_final = format_detected;
        } else if (format_final == SIXEL_PALETTE_FORMAT_PAL_AUTO) {
            format_detected = sixel_palette_guess_format(buffer,
                                                         buffer_size);
            if (format_detected == SIXEL_PALETTE_FORMAT_PAL_JASC ||
                    format_detected == SIXEL_PALETTE_FORMAT_PAL_RIFF) {
                format_final = format_detected;
            } else {
                sixel_helper_set_additional_message(
                    "sixel_prepare_specified_palette: "
                    "ambiguous .pal content.");
                status = SIXEL_BAD_INPUT;
                goto palette_cleanup;
            }
        } else if (need_detection) {
            format_detected = sixel_palette_guess_format(buffer,
                                                         buffer_size);
            if (format_detected == SIXEL_PALETTE_FORMAT_NONE) {
                sixel_helper_set_additional_message(
                    "sixel_prepare_specified_palette: "
                    "unable to detect palette format.");
                status = SIXEL_BAD_INPUT;
                goto palette_cleanup;
            }
            format_final = format_detected;
        }

        switch (format_final) {
        case SIXEL_PALETTE_FORMAT_ACT:
            status = sixel_palette_parse_act(buffer,
                                             buffer_size,
                                             encoder,
                                             dither);
            break;
        case SIXEL_PALETTE_FORMAT_PAL_JASC:
            status = sixel_palette_parse_pal_jasc(buffer,
                                                  buffer_size,
                                                  encoder,
                                                  dither);
            break;
        case SIXEL_PALETTE_FORMAT_PAL_RIFF:
            status = sixel_palette_parse_pal_riff(buffer,
                                                  buffer_size,
                                                  encoder,
                                                  dither);
            break;
        case SIXEL_PALETTE_FORMAT_GPL:
            status = sixel_palette_parse_gpl(buffer,
                                             buffer_size,
                                             encoder,
                                             dither);
            break;
        default:
            sixel_helper_set_additional_message(
                "sixel_prepare_specified_palette: "
                "unsupported palette format.");
            status = SIXEL_BAD_INPUT;
            break;
        }

palette_cleanup:
        if (buffer != NULL) {
            sixel_allocator_free(encoder->allocator, buffer);
            buffer = NULL;
        }
        if (stream != NULL) {
            sixel_palette_close_stream(stream, close_stream);
            stream = NULL;
        }
        if (SIXEL_SUCCEEDED(status)) {
            return status;
        }
        if (!treat_as_image) {
            return status;
        }
    }

    callback_context.reqcolors = encoder->reqcolors;
    callback_context.dither = NULL;
    callback_context.allocator = encoder->allocator;
    callback_context.working_colorspace = encoder->working_colorspace;
    callback_context.lut_policy = encoder->lut_policy;

    sixel_helper_set_loader_trace(encoder->verbose);
    sixel_helper_set_thumbnail_size_hint(
        sixel_encoder_thumbnail_hint(encoder));
    status = sixel_loader_new(&loader, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &fstatic);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_BGCOLOR,
                                 encoder->bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &loop_override);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &encoder->finsecure);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CANCEL_FLAG,
                                 encoder->cancel_flag);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 encoder->loader_order);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 &callback_context);
    if (SIXEL_FAILED(status)) {
        goto end_loader;
    }

    status = sixel_loader_load_file(loader,
                                    encoder->mapfile,
                                    load_image_callback_for_palette);
    if (status != SIXEL_OK) {
        goto end_loader;
    }

end_loader:
    sixel_loader_unref(loader);

    if (status != SIXEL_OK) {
        return status;
    }

    if (! callback_context.dither) {
        sixel_helper_set_additional_message(
            "sixel_prepare_specified_palette() failed.\n"
            "reason: mapfile is empty.");
        return SIXEL_BAD_INPUT;
    }

    *dither = callback_context.dither;

    return status;
}


/* create dither object from a frame */
static SIXELSTATUS
sixel_encoder_prepare_palette(
    sixel_encoder_t *encoder,  /* encoder object */
    sixel_frame_t   *frame,    /* input frame object */
    sixel_dither_t  **dither)  /* dither object to be created from the frame */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int histogram_colors;
    sixel_assessment_t *assessment;
    int promoted_stage;

    assessment = NULL;
    promoted_stage = 0;
    if (encoder != NULL) {
        assessment = encoder->assessment_observer;
    }

    switch (encoder->color_option) {
    case SIXEL_COLOR_OPTION_HIGHCOLOR:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_dither_new(dither, (-1), encoder->allocator);
            sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        }
        goto end;
    case SIXEL_COLOR_OPTION_MONOCHROME:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_monochrome_palette(dither, encoder->finvert);
        }
        goto end;
    case SIXEL_COLOR_OPTION_MAPFILE:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_specified_palette(dither, encoder);
        }
        goto end;
    case SIXEL_COLOR_OPTION_BUILTIN:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_builtin_palette(dither, encoder->builtin_palette);
        }
        goto end;
    case SIXEL_COLOR_OPTION_DEFAULT:
    default:
        break;
    }

    if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE) {
        if (!sixel_frame_get_palette(frame)) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        status = sixel_dither_new(dither, sixel_frame_get_ncolors(frame),
                                  encoder->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_dither_set_palette(*dither, sixel_frame_get_palette(frame));
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        if (sixel_frame_get_transparent(frame) != (-1)) {
            sixel_dither_set_transparent(*dither, sixel_frame_get_transparent(frame));
        }
        if (*dither && encoder->dither_cache) {
            sixel_dither_unref(encoder->dither_cache);
        }
        goto end;
    }

    if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_GRAYSCALE) {
        switch (sixel_frame_get_pixelformat(frame)) {
        case SIXEL_PIXELFORMAT_G1:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G1);
            break;
        case SIXEL_PIXELFORMAT_G2:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G2);
            break;
        case SIXEL_PIXELFORMAT_G4:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G4);
            break;
        case SIXEL_PIXELFORMAT_G8:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G8);
            break;
        default:
            *dither = NULL;
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        if (*dither && encoder->dither_cache) {
            sixel_dither_unref(encoder->dither_cache);
        }
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        status = SIXEL_OK;
        goto end;
    }

    if (encoder->dither_cache) {
        sixel_dither_unref(encoder->dither_cache);
    }
    status = sixel_dither_new(dither, encoder->reqcolors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    sixel_dither_set_lut_policy(*dither, encoder->lut_policy);
    sixel_dither_set_sixel_reversible(*dither,
                                      encoder->sixel_reversible);
    sixel_dither_set_final_merge(*dither, encoder->final_merge_mode);
    (*dither)->quantize_model = encoder->quantize_model;

    status = sixel_dither_initialize(*dither,
                                     sixel_frame_get_pixels(frame),
                                     sixel_frame_get_width(frame),
                                     sixel_frame_get_height(frame),
                                     sixel_frame_get_pixelformat(frame),
                                     encoder->method_for_largest,
                                     encoder->method_for_rep,
                                     encoder->quality_mode);
    if (SIXEL_FAILED(status)) {
        sixel_dither_unref(*dither);
        goto end;
    }

    if (assessment != NULL && promoted_stage == 0) {
        sixel_assessment_stage_transition(
            assessment,
            SIXEL_ASSESSMENT_STAGE_PALETTE_SOLVE);
        promoted_stage = 1;
    }

    histogram_colors = sixel_dither_get_num_of_histogram_colors(*dither);
    if (histogram_colors <= encoder->reqcolors) {
        encoder->method_for_diffuse = SIXEL_DIFFUSE_NONE;
    }
    sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));

    status = SIXEL_OK;

end:
    if (assessment != NULL && promoted_stage == 0) {
        sixel_assessment_stage_transition(
            assessment,
            SIXEL_ASSESSMENT_STAGE_PALETTE_SOLVE);
        promoted_stage = 1;
    }
    if (SIXEL_SUCCEEDED(status) && dither != NULL && *dither != NULL) {
        sixel_dither_set_lut_policy(*dither, encoder->lut_policy);
        /* pass down the user's demand for an exact palette size */
        (*dither)->force_palette = encoder->force_palette;
    }
    return status;
}


/* resize a frame with settings of specified encoder object */
static SIXELSTATUS
sixel_encoder_do_resize(
    sixel_encoder_t /* in */    *encoder,   /* encoder object */
    sixel_frame_t   /* in */    *frame)     /* frame object to be resized */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;

    /* get frame width and height */
    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);

    if (src_width < 1) {
         sixel_helper_set_additional_message(
             "sixel_encoder_do_resize: "
             "detected a frame with a non-positive width.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (src_height < 1) {
         sixel_helper_set_additional_message(
             "sixel_encoder_do_resize: "
             "detected a frame with a non-positive height.");
        return SIXEL_BAD_ARGUMENT;
    }

    /* settings around scaling */
    dst_width = encoder->pixelwidth;    /* may be -1 (default) */
    dst_height = encoder->pixelheight;  /* may be -1 (default) */

    /* if the encoder has percentwidth or percentheight property,
       convert them to pixelwidth / pixelheight */
    if (encoder->percentwidth > 0) {
        dst_width = src_width * encoder->percentwidth / 100;
    }
    if (encoder->percentheight > 0) {
        dst_height = src_height * encoder->percentheight / 100;
    }

    /* if only either width or height is set, set also the other
       to retain frame aspect ratio */
    if (dst_width > 0 && dst_height <= 0) {
        dst_height = src_height * dst_width / src_width;
    }
    if (dst_height > 0 && dst_width <= 0) {
        dst_width = src_width * dst_height / src_height;
    }

    /* do resize */
    if (dst_width > 0 && dst_height > 0) {
        status = sixel_frame_resize(frame, dst_width, dst_height,
                                    encoder->method_for_resampling);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /* success */
    status = SIXEL_OK;

end:
    return status;
}


/* clip a frame with settings of specified encoder object */
static SIXELSTATUS
sixel_encoder_do_clip(
    sixel_encoder_t /* in */    *encoder,   /* encoder object */
    sixel_frame_t   /* in */    *frame)     /* frame object to be resized */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int src_width;
    int src_height;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;

    /* get frame width and height */
    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);

    /* settings around clipping */
    clip_x = encoder->clipx;
    clip_y = encoder->clipy;
    clip_w = encoder->clipwidth;
    clip_h = encoder->clipheight;

    /* adjust clipping width with comparing it to frame width */
    if (clip_w + clip_x > src_width) {
        if (clip_x > src_width) {
            clip_w = 0;
        } else {
            clip_w = src_width - clip_x;
        }
    }

    /* adjust clipping height with comparing it to frame height */
    if (clip_h + clip_y > src_height) {
        if (clip_y > src_height) {
            clip_h = 0;
        } else {
            clip_h = src_height - clip_y;
        }
    }

    /* do clipping */
    if (clip_w > 0 && clip_h > 0) {
        status = sixel_frame_clip(frame, clip_x, clip_y, clip_w, clip_h);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /* success */
    status = SIXEL_OK;

end:
    return status;
}


static void
sixel_debug_print_palette(
    sixel_dither_t /* in */ *dither /* dithering object */
)
{
    unsigned char *palette;
    int i;

    palette = sixel_dither_get_palette(dither);
    fprintf(stderr, "palette:\n");
    for (i = 0; i < sixel_dither_get_num_of_palette_colors(dither); ++i) {
        fprintf(stderr, "%d: #%02x%02x%02x\n", i,
                palette[i * 3 + 0],
                palette[i * 3 + 1],
                palette[i * 3 + 2]);
    }
}


static SIXELSTATUS
sixel_encoder_output_without_macro(
    sixel_frame_t       /* in */ *frame,
    sixel_dither_t      /* in */ *dither,
    sixel_output_t      /* in */ *output,
    sixel_encoder_t     /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    static unsigned char *p;
    int depth;
    enum { message_buffer_size = 2048 };
    char message[message_buffer_size];
    int nwrite;
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
    int dulation;
    int delay;
    struct timespec tv;
#endif
    unsigned char *pixbuf;
    int width;
    int height;
    int pixelformat = 0;
    size_t size;
    int frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    palette_conversion_t palette_ctx;

    memset(&palette_ctx, 0, sizeof(palette_ctx));
#if defined(HAVE_CLOCK) || defined(HAVE_CLOCK_WIN)
    sixel_clock_t last_clock;
#endif

    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: encoder object is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (encoder->assessment_observer != NULL) {
        sixel_assessment_stage_transition(
            encoder->assessment_observer,
            SIXEL_ASSESSMENT_STAGE_PALETTE_APPLY);
    }

    if (encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT) {
        if (encoder->force_palette) {
            /* keep every slot when the user forced the palette size */
            sixel_dither_set_optimize_palette(dither, 0);
        } else {
            sixel_dither_set_optimize_palette(dither, 1);
        }
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    frame_colorspace = sixel_frame_get_colorspace(frame);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    output->colorspace = encoder->output_colorspace;
    sixel_dither_set_pixelformat(dither, pixelformat);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_LOGIC_ERROR;
        nwrite = sixel_compat_snprintf(
            message,
            sizeof(message),
            "sixel_encoder_output_without_macro: "
            "sixel_helper_compute_depth(%08x) failed.",
            pixelformat);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        goto end;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    size = (size_t)(width * height * depth);
    p = (unsigned char *)sixel_allocator_malloc(encoder->allocator, size);
    if (p == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
#if defined(HAVE_CLOCK)
    if (output->last_clock == 0) {
        output->last_clock = clock();
    }
#elif defined(HAVE_CLOCK_WIN)
    if (output->last_clock == 0) {
        output->last_clock = clock_win();
    }
#endif
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
    delay = sixel_frame_get_delay(frame);
    if (delay > 0 && !encoder->fignore_delay) {
# if defined(HAVE_CLOCK)
        last_clock = clock();
/* https://stackoverflow.com/questions/16697005/clock-and-clocks-per-sec-on-osx-10-7 */
#  if defined(__APPLE__)
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / 100000);
#  else
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / CLOCKS_PER_SEC);
#  endif
        output->last_clock = last_clock;
# elif defined(HAVE_CLOCK_WIN)
        last_clock = clock_win();
        dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                          / CLOCKS_PER_SEC);
        output->last_clock = last_clock;
# else
        dulation = 0;
# endif
        if (dulation < 1000 * 10 * delay) {
# if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
            tv.tv_sec = 0;
            tv.tv_nsec = (long)((1000 * 10 * delay - dulation) * 1000);
#  if defined(HAVE_NANOSLEEP)
            nanosleep(&tv, NULL);
#  else
            nanosleep_win(&tv, NULL);
#  endif
# endif
        }
    }
#endif

    pixbuf = sixel_frame_get_pixels(frame);
    memcpy(p, pixbuf, (size_t)(width * height * depth));

    status = sixel_output_convert_colorspace(output, p, size);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        goto end;
    }

    status = sixel_encoder_convert_palette(encoder,
                                           output,
                                           dither,
                                           frame_colorspace,
                                           pixelformat,
                                           &palette_ctx);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (encoder->capture_quantized) {
        status = sixel_encoder_capture_quantized(encoder,
                                                 dither,
                                                 p,
                                                 size,
                                                 width,
                                                 height,
                                                 pixelformat,
                                                 output->colorspace);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (encoder->assessment_observer != NULL) {
        sixel_assessment_stage_transition(
            encoder->assessment_observer,
            SIXEL_ASSESSMENT_STAGE_ENCODE);
    }
    status = sixel_encode(p, width, height, depth, dither, output);
    if (encoder->assessment_observer != NULL) {
        sixel_assessment_stage_finish(encoder->assessment_observer);
    }
    if (status != SIXEL_OK) {
        goto end;
    }

end:
    sixel_encoder_restore_palette(encoder, dither, &palette_ctx);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    sixel_allocator_free(encoder->allocator, p);

    return status;
}


static SIXELSTATUS
sixel_encoder_output_with_macro(
    sixel_frame_t   /* in */ *frame,
    sixel_dither_t  /* in */ *dither,
    sixel_output_t  /* in */ *output,
    sixel_encoder_t /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    enum { message_buffer_size = 256 };
    char buffer[message_buffer_size];
    int nwrite;
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
    int dulation;
    struct timespec tv;
#endif
    int width;
    int height;
    int pixelformat;
    int depth;
    size_t size = 0;
    int frame_colorspace = SIXEL_COLORSPACE_GAMMA;
    unsigned char *converted = NULL;
    palette_conversion_t palette_ctx;
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
    int delay;
#endif
#if defined(HAVE_CLOCK) || defined(HAVE_CLOCK_WIN)
    sixel_clock_t last_clock;
#endif
    double write_started_at;
    double write_finished_at;
    double write_duration;

    memset(&palette_ctx, 0, sizeof(palette_ctx));

    if (encoder != NULL && encoder->assessment_observer != NULL) {
        sixel_assessment_stage_transition(
            encoder->assessment_observer,
            SIXEL_ASSESSMENT_STAGE_PALETTE_APPLY);
    }

#if defined(HAVE_CLOCK)
    if (output->last_clock == 0) {
        output->last_clock = clock();
    }
#elif defined(HAVE_CLOCK_WIN)
    if (output->last_clock == 0) {
        output->last_clock = clock_win();
    }
#endif

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    pixelformat = sixel_frame_get_pixelformat(frame);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_LOGIC_ERROR;
        sixel_helper_set_additional_message(
            "sixel_encoder_output_with_macro: "
            "sixel_helper_compute_depth() failed.");
        goto end;
    }

    frame_colorspace = sixel_frame_get_colorspace(frame);
    size = (size_t)width * (size_t)height * (size_t)depth;
    converted = (unsigned char *)sixel_allocator_malloc(
        encoder->allocator, size);
    if (converted == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_with_macro: "
            "sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    memcpy(converted, sixel_frame_get_pixels(frame), size);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    output->colorspace = encoder->output_colorspace;
    status = sixel_output_convert_colorspace(output, converted, size);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_encoder_convert_palette(encoder,
                                           output,
                                           dither,
                                           frame_colorspace,
                                           pixelformat,
                                           &palette_ctx);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (sixel_frame_get_loop_no(frame) == 0) {
        if (encoder->macro_number >= 0) {
            nwrite = sixel_compat_snprintf(
                buffer,
                sizeof(buffer),
                "\033P%d;0;1!z",
                encoder->macro_number);
        } else {
            nwrite = sixel_compat_snprintf(
                buffer,
                sizeof(buffer),
                "\033P%d;0;1!z",
                sixel_frame_get_frame_no(frame));
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: command format failed.");
            goto end;
        }
        write_started_at = 0.0;
        write_finished_at = 0.0;
        write_duration = 0.0;
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            write_started_at = sixel_assessment_timer_now();
        }
        nwrite = sixel_write_callback(buffer,
                                      (int)strlen(buffer),
                                      &encoder->outfd);
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            write_finished_at = sixel_assessment_timer_now();
            write_duration = write_finished_at - write_started_at;
            if (write_duration < 0.0) {
                write_duration = 0.0;
            }
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            sixel_assessment_record_output_write(
                encoder->assessment_observer,
                (size_t)nwrite,
                write_duration);
        }

        if (encoder != NULL && encoder->assessment_observer != NULL) {
            sixel_assessment_stage_transition(
                encoder->assessment_observer,
                SIXEL_ASSESSMENT_STAGE_ENCODE);
        }
        status = sixel_encode(converted,
                              width,
                              height,
                              depth,
                              dither,
                              output);
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            sixel_assessment_stage_finish(
                encoder->assessment_observer);
        }
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        write_started_at = 0.0;
        write_finished_at = 0.0;
        write_duration = 0.0;
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            write_started_at = sixel_assessment_timer_now();
        }
        nwrite = sixel_write_callback("\033\\", 2, &encoder->outfd);
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            write_finished_at = sixel_assessment_timer_now();
            write_duration = write_finished_at - write_started_at;
            if (write_duration < 0.0) {
                write_duration = 0.0;
            }
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            sixel_assessment_record_output_write(
                encoder->assessment_observer,
                (size_t)nwrite,
                write_duration);
        }
    }
    if (encoder->macro_number < 0) {
        nwrite = sixel_compat_snprintf(
            buffer,
            sizeof(buffer),
            "\033[%d*z",
            sixel_frame_get_frame_no(frame));
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: command format failed.");
        }
        write_started_at = 0.0;
        write_finished_at = 0.0;
        write_duration = 0.0;
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            write_started_at = sixel_assessment_timer_now();
        }
        nwrite = sixel_write_callback(buffer,
                                      (int)strlen(buffer),
                                      &encoder->outfd);
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            write_finished_at = sixel_assessment_timer_now();
            write_duration = write_finished_at - write_started_at;
            if (write_duration < 0.0) {
                write_duration = 0.0;
            }
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: "
                "sixel_write_callback() failed.");
            goto end;
        }
        if (encoder != NULL && encoder->assessment_observer != NULL) {
            sixel_assessment_record_output_write(
                encoder->assessment_observer,
                (size_t)nwrite,
                write_duration);
        }
#if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
        delay = sixel_frame_get_delay(frame);
        if (delay > 0 && !encoder->fignore_delay) {
# if defined(HAVE_CLOCK)
            last_clock = clock();
/* https://stackoverflow.com/questions/16697005/clock-and-clocks-per-sec-on-osx-10-7 */
#  if defined(__APPLE__)
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / 100000);
#  else
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / CLOCKS_PER_SEC);
#  endif
            output->last_clock = last_clock;
# elif defined(HAVE_CLOCK_WIN)
            last_clock = clock_win();
            dulation = (int)((last_clock - output->last_clock) * 1000 * 1000
                             / CLOCKS_PER_SEC);
            output->last_clock = last_clock;
# else
            dulation = 0;
# endif
            if (dulation < 1000 * 10 * delay) {
# if defined(HAVE_NANOSLEEP) || defined(HAVE_NANOSLEEP_WIN)
                tv.tv_sec = 0;
                tv.tv_nsec = (long)((1000 * 10 * delay - dulation) * 1000);
#  if defined(HAVE_NANOSLEEP)
                nanosleep(&tv, NULL);
#  else
                nanosleep_win(&tv, NULL);
#  endif
# endif
            }
        }
#endif
    }

end:
    sixel_encoder_restore_palette(encoder, dither, &palette_ctx);
    output->pixelformat = pixelformat;
    output->source_colorspace = frame_colorspace;
    sixel_allocator_free(encoder->allocator, converted);

    return status;
}


static SIXELSTATUS
sixel_encoder_emit_iso2022_chars(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame
)
{
    char *buf_p, *buf;
    int col, row;
    int charset;
    int is_96cs;
    unsigned int charset_no;
    unsigned int code;
    int num_cols, num_rows;
    SIXELSTATUS status;
    size_t alloc_size;
    int nwrite;
    int target_fd;
    int chunk_size;

    charset_no = encoder->drcs_charset_no;
    if (charset_no == 0u) {
        charset_no = 1u;
    }
    if (encoder->drcs_mmv == 0) {
        is_96cs = (charset_no > 63u) ? 1 : 0;
        charset = (int)(((charset_no - 1u) % 63u) + 0x40u);
    } else if (encoder->drcs_mmv == 1) {
        is_96cs = 0;
        charset = (int)(charset_no + 0x3fu);
    } else {
        is_96cs = (charset_no > 79u) ? 1 : 0;
        charset = (int)(((charset_no - 1u) % 79u) + 0x30u);
    }
    code = 0x100020 + (is_96cs ? 0x80 : 0) + charset * 0x100;
    num_cols = (sixel_frame_get_width(frame) + encoder->cell_width - 1)
             / encoder->cell_width;
    num_rows = (sixel_frame_get_height(frame) + encoder->cell_height - 1)
             / encoder->cell_height;

    /* cols x rows + designation(4 chars) + SI + SO + LFs */
    alloc_size = num_cols * num_rows + (num_cols * num_rows / 96 + 1) * 4 + 2 + num_rows;
    buf_p = buf = sixel_allocator_malloc(encoder->allocator, alloc_size);
    if (buf == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_iso2022_chars: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    code = 0x20;
    *(buf_p++) = '\016';  /* SI */
    *(buf_p++) = '\033';
    *(buf_p++) = ')';
    *(buf_p++) = ' ';
    *(buf_p++) = charset;
    for(row = 0; row < num_rows; row++) {
        for(col = 0; col < num_cols; col++) {
            if ((code & 0x7f) == 0x0) {
                if (charset == 0x7e) {
                    is_96cs = 1 - is_96cs;
                    charset = '0';
                } else {
                    charset++;
                }
                code = 0x20;
                *(buf_p++) = '\033';
                *(buf_p++) = is_96cs ? '-': ')';
                *(buf_p++) = ' ';
                *(buf_p++) = charset;
            }
            *(buf_p++) = code++;
        }
        *(buf_p++) = '\n';
    }
    *(buf_p++) = '\017';  /* SO */

    if (encoder->tile_outfd >= 0) {
        target_fd = encoder->tile_outfd;
    } else {
        target_fd = encoder->outfd;
    }

    chunk_size = (int)(buf_p - buf);
    nwrite = sixel_encoder_probe_fd_write(encoder,
                                          buf,
                                          chunk_size,
                                          target_fd);
    if (nwrite != chunk_size) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_iso2022_chars: write() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    sixel_allocator_free(encoder->allocator, buf);

    status = SIXEL_OK;

end:
    return status;
}


/*
 * This routine is derived from mlterm's drcssixel.c
 * (https://raw.githubusercontent.com/arakiken/mlterm/master/drcssixel/drcssixel.c).
 * The original implementation is credited to Araki Ken.
 * Adjusted here to integrate with libsixel's encoder pipeline.
 */
static SIXELSTATUS
sixel_encoder_emit_drcsmmv2_chars(
    sixel_encoder_t *encoder,
    sixel_frame_t *frame
)
{
    char *buf_p, *buf;
    int col, row;
    int charset;
    int is_96cs;
    unsigned int charset_no;
    unsigned int code;
    int num_cols, num_rows;
    SIXELSTATUS status;
    size_t alloc_size;
    int nwrite;
    int target_fd;
    int chunk_size;

    charset_no = encoder->drcs_charset_no;
    if (charset_no == 0u) {
        charset_no = 1u;
    }
    if (encoder->drcs_mmv == 0) {
        is_96cs = (charset_no > 63u) ? 1 : 0;
        charset = (int)(((charset_no - 1u) % 63u) + 0x40u);
    } else if (encoder->drcs_mmv == 1) {
        is_96cs = 0;
        charset = (int)(charset_no + 0x3fu);
    } else {
        is_96cs = (charset_no > 79u) ? 1 : 0;
        charset = (int)(((charset_no - 1u) % 79u) + 0x30u);
    }
    code = 0x100020 + (is_96cs ? 0x80 : 0) + charset * 0x100;
    num_cols = (sixel_frame_get_width(frame) + encoder->cell_width - 1)
             / encoder->cell_width;
    num_rows = (sixel_frame_get_height(frame) + encoder->cell_height - 1)
             / encoder->cell_height;

    /* cols x rows x 4(out of BMP) + rows(LFs) */
    alloc_size = num_cols * num_rows * 4 + num_rows;
    buf_p = buf = sixel_allocator_malloc(encoder->allocator, alloc_size);
    if (buf == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_drcsmmv2_chars: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    for(row = 0; row < num_rows; row++) {
        for(col = 0; col < num_cols; col++) {
            *(buf_p++) = ((code >> 18) & 0x07) | 0xf0;
            *(buf_p++) = ((code >> 12) & 0x3f) | 0x80;
            *(buf_p++) = ((code >> 6) & 0x3f) | 0x80;
            *(buf_p++) = (code & 0x3f) | 0x80;
            code++;
            if ((code & 0x7f) == 0x0) {
                if (charset == 0x7e) {
                    is_96cs = 1 - is_96cs;
                    charset = '0';
                } else {
                    charset++;
                }
                code = 0x100020 + (is_96cs ? 0x80 : 0) + charset * 0x100;
            }
        }
        *(buf_p++) = '\n';
    }

    if (charset == 0x7e) {
        is_96cs = 1 - is_96cs;
    } else {
        charset = '0';
        charset++;
    }
    if (encoder->tile_outfd >= 0) {
        target_fd = encoder->tile_outfd;
    } else {
        target_fd = encoder->outfd;
    }

    chunk_size = (int)(buf_p - buf);
    nwrite = sixel_encoder_probe_fd_write(encoder,
                                          buf,
                                          chunk_size,
                                          target_fd);
    if (nwrite != chunk_size) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_drcsmmv2_chars: write() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    sixel_allocator_free(encoder->allocator, buf);

    status = SIXEL_OK;

end:
    return status;
}

static SIXELSTATUS
sixel_encoder_encode_frame(
    sixel_encoder_t *encoder,
    sixel_frame_t   *frame,
    sixel_output_t  *output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_dither_t *dither = NULL;
    int height;
    int is_animation = 0;
    int nwrite;
    int drcs_is_96cs_param;
    int drcs_designate_char;
    char buf[256];
    sixel_write_function fn_write;
    sixel_write_function write_callback;
    sixel_write_function scroll_callback;
    void *write_priv;
    void *scroll_priv;
    sixel_encoder_output_probe_t probe;
    sixel_encoder_output_probe_t scroll_probe;
    sixel_assessment_t *assessment;

    fn_write = sixel_write_callback;
    write_callback = sixel_write_callback;
    scroll_callback = sixel_write_callback;
    write_priv = &encoder->outfd;
    scroll_priv = &encoder->outfd;
    probe.encoder = NULL;
    probe.base_write = NULL;
    probe.base_priv = NULL;
    scroll_probe.encoder = NULL;
    scroll_probe.base_write = NULL;
    scroll_probe.base_priv = NULL;
    assessment = NULL;
    if (encoder != NULL) {
        assessment = encoder->assessment_observer;
    }
    if (assessment != NULL) {
        if (encoder->clipfirst) {
            sixel_assessment_stage_transition(
                assessment,
                SIXEL_ASSESSMENT_STAGE_CROP);
        } else {
            sixel_assessment_stage_transition(
                assessment,
                SIXEL_ASSESSMENT_STAGE_SCALE);
        }
    }

    /*
     *  Geometry timeline:
     *
     *      +-------+    +------+    +---------------+
     *      | scale | -> | crop | -> | color convert |
     *      +-------+    +------+    +---------------+
     *
     *  The order of the first two blocks mirrors `-c`, so we hop between
     *  SCALE and CROP depending on `clipfirst`.
     */

    if (encoder->clipfirst) {
        status = sixel_encoder_do_clip(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (assessment != NULL) {
            sixel_assessment_stage_transition(
                assessment,
                SIXEL_ASSESSMENT_STAGE_SCALE);
        }
        status = sixel_encoder_do_resize(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        status = sixel_encoder_do_resize(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (assessment != NULL) {
            sixel_assessment_stage_transition(
                assessment,
                SIXEL_ASSESSMENT_STAGE_CROP);
        }
        status = sixel_encoder_do_clip(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (assessment != NULL) {
        sixel_assessment_stage_transition(
            assessment,
            SIXEL_ASSESSMENT_STAGE_COLORSPACE);
    }

    status = sixel_frame_ensure_colorspace(frame,
                                           encoder->working_colorspace);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (assessment != NULL) {
        sixel_assessment_stage_transition(
            assessment,
            SIXEL_ASSESSMENT_STAGE_PALETTE_HISTOGRAM);
    }

    /* prepare dither context */
    status = sixel_encoder_prepare_palette(encoder, frame, &dither);
    if (status != SIXEL_OK) {
        dither = NULL;
        goto end;
    }

    if (encoder->dither_cache != NULL) {
        encoder->dither_cache = dither;
        sixel_dither_ref(dither);
    }

    if (encoder->fdrcs) {
        status = sixel_encoder_ensure_cell_size(encoder);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (encoder->fuse_macro || encoder->macro_number >= 0) {
            sixel_helper_set_additional_message(
                "drcs option cannot be used together with macro output.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
    }

    /* evaluate -v option: print palette */
    if (encoder->verbose) {
        if ((sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE)) {
            sixel_debug_print_palette(dither);
        }
    }

    /* evaluate -d option: set method for diffusion */
    sixel_dither_set_diffusion_type(dither, encoder->method_for_diffuse);
    sixel_dither_set_diffusion_scan(dither, encoder->method_for_scan);
    sixel_dither_set_diffusion_carry(dither, encoder->method_for_carry);

    /* evaluate -C option: set complexion score */
    if (encoder->complexion > 1) {
        sixel_dither_set_complexion_score(dither, encoder->complexion);
    }

    if (output) {
        sixel_output_ref(output);
        if (encoder->assessment_observer != NULL) {
            probe.encoder = encoder;
            probe.base_write = fn_write;
            probe.base_priv = &encoder->outfd;
            write_callback = sixel_write_with_probe;
            write_priv = &probe;
        }
    } else {
        /* create output context */
        if (encoder->fuse_macro || encoder->macro_number >= 0) {
            /* -u or -n option */
            fn_write = sixel_hex_write_callback;
        } else {
            fn_write = sixel_write_callback;
        }
        write_callback = fn_write;
        write_priv = &encoder->outfd;
        if (encoder->assessment_observer != NULL) {
            probe.encoder = encoder;
            probe.base_write = fn_write;
            probe.base_priv = &encoder->outfd;
            write_callback = sixel_write_with_probe;
            write_priv = &probe;
        }
        status = sixel_output_new(&output,
                                  write_callback,
                                  write_priv,
                                  encoder->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (encoder->fdrcs) {
        sixel_output_set_skip_dcs_envelope(output, 1);
        sixel_output_set_skip_header(output, 1);
    }

    sixel_output_set_8bit_availability(output, encoder->f8bit);
    sixel_output_set_gri_arg_limit(output, encoder->has_gri_arg_limit);
    sixel_output_set_palette_type(output, encoder->palette_type);
    sixel_output_set_penetrate_multiplexer(
        output, encoder->penetrate_multiplexer);
    sixel_output_set_encode_policy(output, encoder->encode_policy);
    sixel_output_set_ormode(output, encoder->ormode);

    if (sixel_frame_get_multiframe(frame) && !encoder->fstatic) {
        if (sixel_frame_get_loop_no(frame) != 0 || sixel_frame_get_frame_no(frame) != 0) {
            is_animation = 1;
        }
        height = sixel_frame_get_height(frame);
        if (encoder->assessment_observer != NULL) {
            scroll_probe.encoder = encoder;
            scroll_probe.base_write = sixel_write_callback;
            scroll_probe.base_priv = &encoder->outfd;
            scroll_callback = sixel_write_with_probe;
            scroll_priv = &scroll_probe;
        } else {
            scroll_callback = sixel_write_callback;
            scroll_priv = &encoder->outfd;
        }
        (void) sixel_tty_scroll(scroll_callback,
                                scroll_priv,
                                encoder->outfd,
                                height,
                                is_animation);
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }

    if (encoder->fdrcs) {  /* -@ option */
        if (encoder->drcs_mmv == 0) {
            drcs_is_96cs_param =
                (encoder->drcs_charset_no > 63u) ? 1 : 0;
            drcs_designate_char =
                (int)(((encoder->drcs_charset_no - 1u) % 63u) + 0x40u);
        } else if (encoder->drcs_mmv == 1) {
            drcs_is_96cs_param = 0;
            drcs_designate_char =
                (int)(encoder->drcs_charset_no + 0x3fu);
        } else {
            drcs_is_96cs_param =
                (encoder->drcs_charset_no > 79u) ? 1 : 0;
            drcs_designate_char =
                (int)(((encoder->drcs_charset_no - 1u) % 79u) + 0x30u);
        }
        nwrite = sprintf(buf,
                         "%s%s1;0;0;%d;1;3;%d;%d{ %c",
                         (encoder->drcs_mmv > 0) ? (
                             encoder->f8bit ? "\233?8800h": "\033[?8800h"
                         ): "",
                         encoder->f8bit ? "\220": "\033P",
                         encoder->cell_width,
                         encoder->cell_height,
                         drcs_is_96cs_param,
                         drcs_designate_char);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: command format failed.");
            goto end;
        }
        nwrite = write_callback(buf, nwrite, write_priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: write() failed.");
            goto end;
        }
    }

    /* output sixel: junction of multi-frame processing strategy */
    if (encoder->fuse_macro) {  /* -u option */
        /* use macro */
        status = sixel_encoder_output_with_macro(frame, dither, output, encoder);
    } else if (encoder->macro_number >= 0) { /* -n option */
        /* use macro */
        status = sixel_encoder_output_with_macro(frame, dither, output, encoder);
    } else {
        /* do not use macro */
        status = sixel_encoder_output_without_macro(frame, dither, output, encoder);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        nwrite = write_callback("\x18\033\\", 3, write_priv);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: write_callback() failed.");
            goto end;
        }
        status = SIXEL_INTERRUPTED;
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    if (encoder->fdrcs) {  /* -@ option */
        if (encoder->f8bit) {
            nwrite = write_callback("\234", 1, write_priv);
        } else {
            nwrite = write_callback("\033\\", 2, write_priv);
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_encode_frame: write_callback() failed.");
            goto end;
        }

        if (encoder->tile_outfd >= 0) {
            if (encoder->drcs_mmv == 0) {
                status = sixel_encoder_emit_iso2022_chars(encoder, frame);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            } else {
                status = sixel_encoder_emit_drcsmmv2_chars(encoder, frame);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }
        }
    }


end:
    if (output) {
        sixel_output_unref(output);
    }
    if (dither) {
        sixel_dither_unref(dither);
    }

    return status;
}


/* create encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_new(
    sixel_encoder_t     /* out */ **ppencoder, /* encoder object to be created */
    sixel_allocator_t   /* in */  *allocator)  /* allocator, null if you use
                                                  default allocator */
{
    SIXELSTATUS status = SIXEL_FALSE;
    char const *env_default_bgcolor = NULL;
    char const *env_default_ncolors = NULL;
    int ncolors;
#if HAVE__DUPENV_S
    errno_t e;
    size_t len;
#endif  /* HAVE__DUPENV_S */

    if (allocator == NULL) {
        status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(allocator);
    }

    *ppencoder
        = (sixel_encoder_t *)sixel_allocator_malloc(allocator,
                                                    sizeof(sixel_encoder_t));
    if (*ppencoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_allocator_unref(allocator);
        goto end;
    }

    (*ppencoder)->ref                   = 1;
    (*ppencoder)->reqcolors             = (-1);
    (*ppencoder)->force_palette         = 0;
    (*ppencoder)->mapfile               = NULL;
    (*ppencoder)->palette_output        = NULL;
    (*ppencoder)->loader_order          = NULL;
    (*ppencoder)->color_option          = SIXEL_COLOR_OPTION_DEFAULT;
    (*ppencoder)->builtin_palette       = 0;
    (*ppencoder)->method_for_diffuse    = SIXEL_DIFFUSE_AUTO;
    (*ppencoder)->method_for_scan       = SIXEL_SCAN_AUTO;
    (*ppencoder)->method_for_carry      = SIXEL_CARRY_AUTO;
    (*ppencoder)->method_for_largest    = SIXEL_LARGE_AUTO;
    (*ppencoder)->method_for_rep        = SIXEL_REP_AUTO;
    (*ppencoder)->quality_mode          = SIXEL_QUALITY_AUTO;
    (*ppencoder)->quantize_model        = SIXEL_QUANTIZE_MODEL_AUTO;
    (*ppencoder)->final_merge_mode      = SIXEL_FINAL_MERGE_AUTO;
    (*ppencoder)->lut_policy            = SIXEL_LUT_POLICY_AUTO;
    (*ppencoder)->sixel_reversible      = 0;
    (*ppencoder)->method_for_resampling = SIXEL_RES_BILINEAR;
    (*ppencoder)->loop_mode             = SIXEL_LOOP_AUTO;
    (*ppencoder)->palette_type          = SIXEL_PALETTETYPE_AUTO;
    (*ppencoder)->f8bit                 = 0;
    (*ppencoder)->has_gri_arg_limit     = 0;
    (*ppencoder)->finvert               = 0;
    (*ppencoder)->fuse_macro            = 0;
    (*ppencoder)->fdrcs                 = 0;
    (*ppencoder)->fignore_delay         = 0;
    (*ppencoder)->complexion            = 1;
    (*ppencoder)->fstatic               = 0;
    (*ppencoder)->cell_width            = 0;
    (*ppencoder)->cell_height           = 0;
    (*ppencoder)->pixelwidth            = (-1);
    (*ppencoder)->pixelheight           = (-1);
    (*ppencoder)->percentwidth          = (-1);
    (*ppencoder)->percentheight         = (-1);
    (*ppencoder)->clipx                 = 0;
    (*ppencoder)->clipy                 = 0;
    (*ppencoder)->clipwidth             = 0;
    (*ppencoder)->clipheight            = 0;
    (*ppencoder)->clipfirst             = 0;
    (*ppencoder)->macro_number          = (-1);
    (*ppencoder)->verbose               = 0;
    (*ppencoder)->penetrate_multiplexer = 0;
    (*ppencoder)->encode_policy         = SIXEL_ENCODEPOLICY_AUTO;
    (*ppencoder)->working_colorspace    = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->output_colorspace     = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->ormode                = 0;
    (*ppencoder)->pipe_mode             = 0;
    (*ppencoder)->bgcolor               = NULL;
    (*ppencoder)->outfd                 = STDOUT_FILENO;
    (*ppencoder)->tile_outfd            = (-1);
    (*ppencoder)->finsecure             = 0;
    (*ppencoder)->cancel_flag           = NULL;
    (*ppencoder)->dither_cache          = NULL;
    (*ppencoder)->drcs_charset_no       = 1u;
    (*ppencoder)->drcs_mmv              = 2;
    (*ppencoder)->capture_quantized     = 0;
    (*ppencoder)->capture_source        = 0;
    (*ppencoder)->capture_pixels        = NULL;
    (*ppencoder)->capture_pixels_size   = 0;
    (*ppencoder)->capture_palette       = NULL;
    (*ppencoder)->capture_palette_size  = 0;
    (*ppencoder)->capture_pixel_bytes   = 0;
    (*ppencoder)->capture_width         = 0;
    (*ppencoder)->capture_height        = 0;
    (*ppencoder)->capture_pixelformat   = SIXEL_PIXELFORMAT_RGB888;
    (*ppencoder)->capture_colorspace    = SIXEL_COLORSPACE_GAMMA;
    (*ppencoder)->capture_ncolors       = 0;
    (*ppencoder)->capture_valid         = 0;
    (*ppencoder)->capture_source_frame  = NULL;
    (*ppencoder)->assessment_observer   = NULL;
    (*ppencoder)->assessment_json_path  = NULL;
    (*ppencoder)->assessment_sections   = SIXEL_ASSESSMENT_SECTION_NONE;
    (*ppencoder)->last_loader_name[0]   = '\0';
    (*ppencoder)->last_source_path[0]   = '\0';
    (*ppencoder)->last_input_bytes      = 0u;
    (*ppencoder)->output_is_png         = 0;
    (*ppencoder)->output_png_to_stdout  = 0;
    (*ppencoder)->png_output_path       = NULL;
    (*ppencoder)->sixel_output_path     = NULL;
    (*ppencoder)->clipboard_output_active = 0;
    (*ppencoder)->clipboard_output_format[0] = '\0';
    (*ppencoder)->clipboard_output_path = NULL;
    (*ppencoder)->allocator             = allocator;

    /* evaluate environment variable ${SIXEL_BGCOLOR} */
#if HAVE__DUPENV_S
    e = _dupenv_s(&env_default_bgcolor, &len, "SIXEL_BGCOLOR");
    if (e != (0)) {
        sixel_helper_set_additional_message(
            "failed to get environment variable $SIXEL_BGCOLOR.");
        return (SIXEL_LIBC_ERROR | (e & 0xff));
    }
#else
    env_default_bgcolor = sixel_compat_getenv("SIXEL_BGCOLOR");
#endif  /* HAVE__DUPENV_S */
    if (env_default_bgcolor != NULL) {
        status = sixel_parse_x_colorspec(&(*ppencoder)->bgcolor,
                                         env_default_bgcolor,
                                         allocator);
        if (SIXEL_FAILED(status)) {
            goto error;
        }
    }

    /* evaluate environment variable ${SIXEL_COLORS} */
#if HAVE__DUPENV_S
    e = _dupenv_s(&env_default_bgcolor, &len, "SIXEL_COLORS");
    if (e != (0)) {
        sixel_helper_set_additional_message(
            "failed to get environment variable $SIXEL_COLORS.");
        return (SIXEL_LIBC_ERROR | (e & 0xff));
    }
#else
    env_default_ncolors = sixel_compat_getenv("SIXEL_COLORS");
#endif  /* HAVE__DUPENV_S */
    if (env_default_ncolors) {
        ncolors = atoi(env_default_ncolors); /* may overflow */
        if (ncolors > 1 && ncolors <= SIXEL_PALETTE_MAX) {
            (*ppencoder)->reqcolors = ncolors;
        }
    }

    /* success */
    status = SIXEL_OK;

    goto end;

error:
    sixel_allocator_free(allocator, *ppencoder);
    sixel_allocator_unref(allocator);
    *ppencoder = NULL;

end:
#if HAVE__DUPENV_S
    free(env_default_bgcolor);
    free(env_default_ncolors);
#endif  /* HAVE__DUPENV_S */
    return status;
}


/* create encoder object (deprecated version) */
SIXELAPI /* deprecated */ sixel_encoder_t *
sixel_encoder_create(void)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_encoder_t *encoder = NULL;

    status = sixel_encoder_new(&encoder, NULL);
    if (SIXEL_FAILED(status)) {
        return NULL;
    }

    return encoder;
}


/* destroy encoder object */
static void
sixel_encoder_destroy(sixel_encoder_t *encoder)
{
    sixel_allocator_t *allocator;

    if (encoder) {
        allocator = encoder->allocator;
        sixel_allocator_free(allocator, encoder->mapfile);
        sixel_allocator_free(allocator, encoder->palette_output);
        sixel_allocator_free(allocator, encoder->loader_order);
        sixel_allocator_free(allocator, encoder->bgcolor);
        sixel_dither_unref(encoder->dither_cache);
        if (encoder->outfd
            && encoder->outfd != STDOUT_FILENO
            && encoder->outfd != STDERR_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
        }
        if (encoder->tile_outfd >= 0
            && encoder->tile_outfd != encoder->outfd
            && encoder->tile_outfd != STDOUT_FILENO
            && encoder->tile_outfd != STDERR_FILENO) {
            (void)sixel_compat_close(encoder->tile_outfd);
        }
        if (encoder->capture_source_frame != NULL) {
            sixel_frame_unref(encoder->capture_source_frame);
        }
        if (encoder->clipboard_output_path != NULL) {
            (void)sixel_compat_unlink(encoder->clipboard_output_path);
            encoder->clipboard_output_path = NULL;
        }
        encoder->clipboard_output_active = 0;
        encoder->clipboard_output_format[0] = '\0';
        sixel_allocator_free(allocator, encoder->capture_pixels);
        sixel_allocator_free(allocator, encoder->capture_palette);
        sixel_allocator_free(allocator, encoder->png_output_path);
        sixel_allocator_free(allocator, encoder->sixel_output_path);
        sixel_allocator_free(allocator, encoder);
        sixel_allocator_unref(allocator);
    }
}


/* increase reference count of encoder object (thread-unsafe) */
SIXELAPI void
sixel_encoder_ref(sixel_encoder_t *encoder)
{
    /* TODO: be thread safe */
    ++encoder->ref;
}


/* decrease reference count of encoder object (thread-unsafe) */
SIXELAPI void
sixel_encoder_unref(sixel_encoder_t *encoder)
{
    /* TODO: be thread safe */
    if (encoder != NULL && --encoder->ref == 0) {
        sixel_encoder_destroy(encoder);
    }
}


/* set cancel state flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_set_cancel_flag(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ *cancel_flag
)
{
    SIXELSTATUS status = SIXEL_OK;

    encoder->cancel_flag = cancel_flag;

    return status;
}


/*
 * parse_assessment_sections() translates a comma-separated section list into
 * the bitmask understood by the assessment back-end.  The accepted grammar is
 * intentionally small so that the CLI contract stays predictable:
 *
 *     list := section {"," section}
 *     section := name | name "@" view
 *
 * Each name maps to a bit flag while the optional view toggles the encoded vs
 * quantized quality comparison.  The helper folds case, trims ASCII
 * whitespace, and rejects mixed encoded/quantized requests so that the caller
 * can rely on a single coherent capture strategy.
 */
static int
parse_assessment_sections(char const *spec,
                          unsigned int *out_sections)
{
    char *copy;
    char *cursor;
    char *token;
    char *context;
    unsigned int sections;
    unsigned int view;
    int result;
    size_t length;
    size_t spec_len;
    char *at;
    char *base;
    char *variant;
    char *p;

    if (spec == NULL || out_sections == NULL) {
        return -1;
    }
    spec_len = strlen(spec);
    copy = (char *)malloc(spec_len + 1u);
    if (copy == NULL) {
        return -1;
    }
    memcpy(copy, spec, spec_len + 1u);
    cursor = copy;
    sections = 0u;
    view = SIXEL_ASSESSMENT_VIEW_ENCODED;
    result = 0;
    context = NULL;
    while (result == 0) {
        token = sixel_compat_strtok(cursor, ",", &context);
        if (token == NULL) {
            break;
        }
        cursor = NULL;
        while (*token == ' ' || *token == '\t') {
            token += 1;
        }
        length = strlen(token);
        while (length > 0u &&
               (token[length - 1u] == ' ' || token[length - 1u] == '\t')) {
            token[length - 1u] = '\0';
            length -= 1u;
        }
        if (*token == '\0') {
            result = -1;
            break;
        }
        for (p = token; *p != '\0'; ++p) {
            *p = (char)tolower((unsigned char)*p);
        }
        at = strchr(token, '@');
        if (at != NULL) {
            *at = '\0';
            variant = at + 1;
            if (*variant == '\0') {
                result = -1;
                break;
            }
        } else {
            variant = NULL;
        }
        base = token;
        if (strcmp(base, "all") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_ALL;
            if (variant != NULL && variant[0] != '\0') {
                if (strcmp(variant, "quantized") == 0) {
                    if (view != SIXEL_ASSESSMENT_VIEW_ENCODED &&
                            view != SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                        result = -1;
                    }
                    view = SIXEL_ASSESSMENT_VIEW_QUANTIZED;
                } else if (strcmp(variant, "encoded") == 0) {
                    if (view == SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                        result = -1;
                    }
                    view = SIXEL_ASSESSMENT_VIEW_ENCODED;
                } else {
                    result = -1;
                }
            }
        } else if (strcmp(base, "basic") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_BASIC;
            if (variant != NULL) {
                result = -1;
            }
        } else if (strcmp(base, "performance") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_PERFORMANCE;
            if (variant != NULL) {
                result = -1;
            }
        } else if (strcmp(base, "size") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_SIZE;
            if (variant != NULL) {
                result = -1;
            }
        } else if (strcmp(base, "quality") == 0) {
            sections |= SIXEL_ASSESSMENT_SECTION_QUALITY;
            if (variant != NULL && variant[0] != '\0') {
                if (strcmp(variant, "quantized") == 0) {
                    if (view != SIXEL_ASSESSMENT_VIEW_ENCODED &&
                            view != SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                        result = -1;
                    }
                    view = SIXEL_ASSESSMENT_VIEW_QUANTIZED;
                } else if (strcmp(variant, "encoded") == 0) {
                    if (view == SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                        result = -1;
                    }
                    view = SIXEL_ASSESSMENT_VIEW_ENCODED;
                } else {
                    result = -1;
                }
            } else if (variant != NULL) {
                if (view == SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                    result = -1;
                }
                view = SIXEL_ASSESSMENT_VIEW_ENCODED;
            }
        } else {
            result = -1;
        }
    }
    if (result == 0) {
        if (sections == 0u) {
            result = -1;
        } else {
            if ((sections & SIXEL_ASSESSMENT_SECTION_QUALITY) != 0u &&
                    view == SIXEL_ASSESSMENT_VIEW_QUANTIZED) {
                sections |= SIXEL_ASSESSMENT_VIEW_QUANTIZED;
            }
            *out_sections = sections;
        }
    }
    free(copy);
    return result;
}


static int
is_png_target(char const *path)
{
    size_t len;
    int matched;

    /*
     * Detect PNG requests from explicit prefixes or a ".png" suffix:
     *
     *   argument
     *   |
     *   v
     *   .............. . p n g
     *   ^             ^^^^^^^^^
     *   |             +-- case-insensitive suffix comparison
     *   +-- accepts the "png:" inline prefix used for stdout capture
     */

    len = 0;
    matched = 0;

    if (path == NULL) {
        return 0;
    }

    if (strncmp(path, "png:", 4) == 0) {
        return path[4] != '\0';
    }

    len = strlen(path);
    if (len >= 4) {
        matched = (tolower((unsigned char)path[len - 4]) == '.')
            && (tolower((unsigned char)path[len - 3]) == 'p')
            && (tolower((unsigned char)path[len - 2]) == 'n')
            && (tolower((unsigned char)path[len - 1]) == 'g');
    }

    return matched;
}


static char const *
png_target_payload_view(char const *argument)
{
    /*
     * Inline PNG targets split into either a prefix/payload pair or rely on
     * a simple file-name suffix:
     *
     *   +--------------+------------+-------------+
     *   | form         | payload    | destination |
     *   +--------------+------------+-------------+
     *   | png:         | -          | stdout      |
     *   | png:         | filename   | filesystem  |
     *   | *.png        | filename   | filesystem  |
     *   +--------------+------------+-------------+
     *
     * The caller only needs the payload column, so we expose it here.  When
     * the user omits the prefix we simply echo the original pointer so the
     * caller can copy the value verbatim.
     */
    if (argument == NULL) {
        return NULL;
    }
    if (strncmp(argument, "png:", 4) == 0) {
        return argument + 4;
    }

    return argument;
}

static void
normalise_windows_drive_path(char *path)
{
#if defined(_WIN32)
    size_t length;

    /*
     * MSYS-like environments forward POSIX-looking absolute paths to native
     * binaries.  When a user writes "/d/..." MSYS converts the command line to
     * UTF-16 and preserves the literal bytes.  The Windows CRT, however,
     * expects "d:/..." or "d:\...".  The tiny state machine below rewrites the
     * leading token so the runtime resolves the drive correctly:
     *
     *   input     normalised
     *   |         |
     *   v         v
     *   / d / ... d : / ...
     *
     * The body keeps the rest of the string intact so UNC paths ("//server")
     * and relative references pass through untouched.
     */

    length = 0u;

    if (path == NULL) {
        return;
    }

    length = strlen(path);
    if (length >= 3u
            && path[0] == '/'
            && ((path[1] >= 'A' && path[1] <= 'Z')
                || (path[1] >= 'a' && path[1] <= 'z'))
            && path[2] == '/') {
        path[0] = path[1];
        path[1] = ':';
    }
#else
    (void)path;
#endif
}


static int
is_dev_null_path(char const *path)
{
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
#if defined(_WIN32)
    if (_stricmp(path, "nul") == 0) {
        return 1;
    }
#endif
    return strcmp(path, "/dev/null") == 0;
}


/* set an option flag to encoder object */
SIXELAPI SIXELSTATUS
sixel_encoder_setopt(
    sixel_encoder_t /* in */ *encoder,
    int             /* in */ arg,
    char const      /* in */ *value)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int number;
    int parsed;
    char unit[32];
    char lowered[16];
    size_t len;
    size_t i;
    long parsed_reqcolors;
    char *endptr;
    int forced_palette;
    char *opt_copy;
    char const *drcs_arg_delim;
    char const *drcs_arg_charset;
    char const *drcs_arg_second_delim;
    char const *drcs_arg_path;
    size_t drcs_arg_path_length;
    size_t drcs_segment_length;
    char drcs_segment[32];
    int drcs_mmv_value;
    long drcs_charset_value;
    unsigned int drcs_charset_limit;
    sixel_option_choice_result_t match_result;
    int match_value;
    char match_detail[128];
    char match_message[256];
    int png_argument_has_prefix = 0;
    char const *png_path_view = NULL;
    size_t png_path_length;

    sixel_encoder_ref(encoder);
    opt_copy = NULL;

    switch(arg) {
    case SIXEL_OPTFLAG_OUTFILE:  /* o */
        if (*value == '\0') {
            sixel_helper_set_additional_message(
                "no file name specified.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (is_png_target(value)) {
            encoder->output_is_png = 1;
            png_argument_has_prefix =
                (value != NULL)
                && (strncmp(value, "png:", 4) == 0);
            png_path_view = png_target_payload_view(value);
            if (png_argument_has_prefix
                    && (png_path_view == NULL
                        || png_path_view[0] == '\0')) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: missing target after the \"png:\" "
                    "prefix. use png:- or png:<path> with a non-empty payload."
                );
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            encoder->output_png_to_stdout =
                (png_path_view != NULL)
                && (strcmp(png_path_view, "-") == 0);
            sixel_allocator_free(encoder->allocator, encoder->png_output_path);
            encoder->png_output_path = NULL;
            sixel_allocator_free(encoder->allocator, encoder->sixel_output_path);
            encoder->sixel_output_path = NULL;
            if (! encoder->output_png_to_stdout) {
                /*
                 * +-----------------------------------------+
                 * |  PNG target normalization               |
                 * +-----------------------------------------+
                 * |  Raw input  |  Stored file path         |
                 * |-------------+---------------------------|
                 * |  png:-      |  "-" (stdout sentinel)    |
                 * |  png:/foo   |  "/foo"                   |
                 * +-----------------------------------------+
                 * Strip the "png:" prefix so the decoder can
                 * pass the true filesystem path to libpng
                 * while the CLI retains its shorthand.
                 */
                png_path_view = value;
                if (strncmp(value, "png:", 4) == 0) {
                    png_path_view = value + 4;
                }
                if (png_path_view[0] == '\0') {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: PNG output path is empty.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                png_path_length = strlen(png_path_view);
                encoder->png_output_path =
                    (char *)sixel_allocator_malloc(
                        encoder->allocator, png_path_length + 1u);
                if (encoder->png_output_path == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: sixel_allocator_malloc() "
                        "failed for PNG output path.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                if (png_path_view != NULL) {
                    (void)sixel_compat_strcpy(encoder->png_output_path,
                                              png_path_length + 1u,
                                              png_path_view);
                } else {
                    encoder->png_output_path[0] = '\0';
                }
                normalise_windows_drive_path(encoder->png_output_path);
            }
        } else {
            encoder->output_is_png = 0;
            encoder->output_png_to_stdout = 0;
            png_argument_has_prefix = 0;
            png_path_view = NULL;
            sixel_allocator_free(encoder->allocator, encoder->png_output_path);
            encoder->png_output_path = NULL;
            sixel_allocator_free(encoder->allocator, encoder->sixel_output_path);
            encoder->sixel_output_path = NULL;
            if (encoder->clipboard_output_path != NULL) {
                (void)sixel_compat_unlink(encoder->clipboard_output_path);
                sixel_allocator_free(encoder->allocator,
                                     encoder->clipboard_output_path);
                encoder->clipboard_output_path = NULL;
            }
            encoder->clipboard_output_active = 0;
            encoder->clipboard_output_format[0] = '\0';
            {
                sixel_clipboard_spec_t clipboard_spec;
                SIXELSTATUS clip_status;
                char *spool_path;
                int spool_fd;

                clipboard_spec.is_clipboard = 0;
                clipboard_spec.format[0] = '\0';
                clip_status = SIXEL_OK;
                spool_path = NULL;
                spool_fd = (-1);

                if (sixel_clipboard_parse_spec(value, &clipboard_spec)
                        && clipboard_spec.is_clipboard) {
                    clip_status = clipboard_create_spool(
                        encoder->allocator,
                        "clipboard-out",
                        &spool_path,
                        &spool_fd);
                    if (SIXEL_FAILED(clip_status)) {
                        status = clip_status;
                        goto end;
                    }
                    clipboard_select_format(
                        encoder->clipboard_output_format,
                        sizeof(encoder->clipboard_output_format),
                        clipboard_spec.format,
                        "sixel");
                    if (encoder->outfd
                            && encoder->outfd != STDOUT_FILENO
                            && encoder->outfd != STDERR_FILENO) {
                        (void)sixel_compat_close(encoder->outfd);
                    }
                    encoder->outfd = spool_fd;
                    spool_fd = (-1);
                    encoder->sixel_output_path = spool_path;
                    encoder->clipboard_output_path = spool_path;
                    spool_path = NULL;
                    encoder->clipboard_output_active = 1;
                    break;
                }

                if (spool_fd >= 0) {
                    (void)sixel_compat_close(spool_fd);
                }
                if (spool_path != NULL) {
                    sixel_allocator_free(encoder->allocator, spool_path);
                }
            }
            if (strcmp(value, "-") != 0) {
                encoder->sixel_output_path = (char *)sixel_allocator_malloc(
                    encoder->allocator, strlen(value) + 1);
                if (encoder->sixel_output_path == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: malloc() failed for output path.");
                    status = SIXEL_BAD_ALLOCATION;
                    goto end;
                }
                (void)sixel_compat_strcpy(encoder->sixel_output_path,
                                          strlen(value) + 1,
                                          value);
            }
        }

        if (!encoder->clipboard_output_active && strcmp(value, "-") != 0) {
            if (encoder->outfd && encoder->outfd != STDOUT_FILENO) {
                (void)sixel_compat_close(encoder->outfd);
            }
            encoder->outfd = sixel_compat_open(value,
                                               O_RDWR | O_CREAT | O_TRUNC,
                                               S_IRUSR | S_IWUSR);
        }
        break;
    case SIXEL_OPTFLAG_ASSESSMENT:  /* a */
        if (parse_assessment_sections(value,
                                      &encoder->assessment_sections) != 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: cannot parse assessment section list");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_ASSESSMENT_FILE:  /* J */
        encoder->assessment_json_path = value;
        break;
    case SIXEL_OPTFLAG_7BIT_MODE:  /* 7 */
        encoder->f8bit = 0;
        break;
    case SIXEL_OPTFLAG_8BIT_MODE:  /* 8 */
        encoder->f8bit = 1;
        break;
    case SIXEL_OPTFLAG_6REVERSIBLE:  /* 6 */
        encoder->sixel_reversible = 1;
        break;
    case SIXEL_OPTFLAG_HAS_GRI_ARG_LIMIT:  /* R */
        encoder->has_gri_arg_limit = 1;
        break;
    case SIXEL_OPTFLAG_COLORS:  /* p */
        forced_palette = 0;
        errno = 0;
        endptr = NULL;
        if (*value == '!' && value[1] == '\0') {
            /*
             * Force the default palette size even when the median cut
             * finished early.
             *
             *   requested colors
             *          |
             *          v
             *        [ 256 ]  <--- "-p!" triggers this shortcut
             */
            parsed_reqcolors = SIXEL_PALETTE_MAX;
            forced_palette = 1;
        } else {
            parsed_reqcolors = strtol(value, &endptr, 10);
            if (endptr != NULL && *endptr == '!') {
                forced_palette = 1;
                ++endptr;
            }
            if (errno == ERANGE || endptr == value) {
                sixel_helper_set_additional_message(
                    "cannot parse -p/--colors option.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            if (endptr != NULL && *endptr != '\0') {
                sixel_helper_set_additional_message(
                    "cannot parse -p/--colors option.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        if (parsed_reqcolors < 1) {
            sixel_helper_set_additional_message(
                "-p/--colors parameter must be 1 or more.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (parsed_reqcolors > SIXEL_PALETTE_MAX) {
            sixel_helper_set_additional_message(
                "-p/--colors parameter must be less then or equal to 256.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->reqcolors = (int)parsed_reqcolors;
        encoder->force_palette = forced_palette;
        break;
    case SIXEL_OPTFLAG_MAPFILE:  /* m */
        if (encoder->mapfile) {
            sixel_allocator_free(encoder->allocator, encoder->mapfile);
        }
        encoder->mapfile = arg_strdup(value, encoder->allocator);
        if (encoder->mapfile == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        encoder->color_option = SIXEL_COLOR_OPTION_MAPFILE;
        break;
    case SIXEL_OPTFLAG_MAPFILE_OUTPUT:  /* M */
        if (value == NULL || *value == '\0') {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: mapfile-output path is empty.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        opt_copy = arg_strdup(value, encoder->allocator);
        if (opt_copy == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        status = sixel_encoder_enable_quantized_capture(encoder, 1);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(encoder->allocator, opt_copy);
            goto end;
        }
        sixel_allocator_free(encoder->allocator, encoder->palette_output);
        encoder->palette_output = opt_copy;
        opt_copy = NULL;
        break;
    case SIXEL_OPTFLAG_MONOCHROME:  /* e */
        encoder->color_option = SIXEL_COLOR_OPTION_MONOCHROME;
        break;
    case SIXEL_OPTFLAG_HIGH_COLOR:  /* I */
        encoder->color_option = SIXEL_COLOR_OPTION_HIGHCOLOR;
        break;
    case SIXEL_OPTFLAG_BUILTIN_PALETTE:  /* b */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_builtin_palette,
            sizeof(g_option_choices_builtin_palette) /
            sizeof(g_option_choices_builtin_palette[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->builtin_palette = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--builtin-palette",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "cannot parse builtin palette option.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->color_option = SIXEL_COLOR_OPTION_BUILTIN;
        break;
    case SIXEL_OPTFLAG_DIFFUSION:  /* d */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_diffusion,
            sizeof(g_option_choices_diffusion) /
            sizeof(g_option_choices_diffusion[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_diffuse = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--diffusion",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "specified diffusion method is not supported.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_DIFFUSION_SCAN:  /* y */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_diffusion_scan,
            sizeof(g_option_choices_diffusion_scan) /
            sizeof(g_option_choices_diffusion_scan[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_scan = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--diffusion-scan",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "specified diffusion scan is not supported.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_DIFFUSION_CARRY:  /* Y */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_diffusion_carry,
            sizeof(g_option_choices_diffusion_carry) /
            sizeof(g_option_choices_diffusion_carry[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_carry = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--diffusion-carry",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "specified diffusion carry mode is not supported.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_FIND_LARGEST:  /* f */
        if (value != NULL) {
            match_result = sixel_match_option_choice(
                value,
                g_option_choices_find_largest,
                sizeof(g_option_choices_find_largest) /
                sizeof(g_option_choices_find_largest[0]),
                &match_value,
                match_detail,
                sizeof(match_detail));
            if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
                encoder->method_for_largest = match_value;
            } else {
                if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                    sixel_report_ambiguous_prefix("--find-largest",
                                                  value,
                                                  match_detail,
                                                  match_message,
                                                  sizeof(match_message));
                } else {
                    sixel_helper_set_additional_message(
                        "specified finding method is not supported.");
                }
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_SELECT_COLOR:  /* s */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_select_color,
            sizeof(g_option_choices_select_color) /
            sizeof(g_option_choices_select_color[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_rep = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--select-color",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "specified finding method is not supported.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_QUANTIZE_MODEL:  /* Q */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_quantize_model,
            sizeof(g_option_choices_quantize_model) /
            sizeof(g_option_choices_quantize_model[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->quantize_model = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--quantize-model",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: unsupported quantize model.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_FINAL_MERGE:  /* F */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_final_merge,
            sizeof(g_option_choices_final_merge) /
            sizeof(g_option_choices_final_merge[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->final_merge_mode = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--final-merge",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "specified final merge policy is not supported.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_CROP:  /* c */
#if HAVE_SSCANF_S
        number = sscanf_s(value, "%dx%d+%d+%d",
                          &encoder->clipwidth, &encoder->clipheight,
                          &encoder->clipx, &encoder->clipy);
#else
        number = sscanf(value, "%dx%d+%d+%d",
                        &encoder->clipwidth, &encoder->clipheight,
                        &encoder->clipx, &encoder->clipy);
#endif  /* HAVE_SSCANF_S */
        if (number != 4) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipwidth <= 0 || encoder->clipheight <= 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipx < 0 || encoder->clipy < 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->clipfirst = 0;
        break;
    case SIXEL_OPTFLAG_WIDTH:  /* w */
#if HAVE_SSCANF_S
        parsed = sscanf_s(value, "%d%2s", &number, unit, sizeof(unit) - 1);
#else
        parsed = sscanf(value, "%d%2s", &number, unit);
#endif  /* HAVE_SSCANF_S */
        if (parsed == 2 && strcmp(unit, "%") == 0) {
            encoder->pixelwidth = (-1);
            encoder->percentwidth = number;
        } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
            encoder->pixelwidth = number;
            encoder->percentwidth = (-1);
        } else if (strcmp(value, "auto") == 0) {
            encoder->pixelwidth = (-1);
            encoder->percentwidth = (-1);
        } else {
            sixel_helper_set_additional_message(
                "cannot parse -w/--width option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipwidth) {
            encoder->clipfirst = 1;
        }
        break;
    case SIXEL_OPTFLAG_HEIGHT:  /* h */
#if HAVE_SSCANF_S
        parsed = sscanf_s(value, "%d%2s", &number, unit, sizeof(unit) - 1);
#else
        parsed = sscanf(value, "%d%2s", &number, unit);
#endif  /* HAVE_SSCANF_S */
        if (parsed == 2 && strcmp(unit, "%") == 0) {
            encoder->pixelheight = (-1);
            encoder->percentheight = number;
        } else if (parsed == 1 || (parsed == 2 && strcmp(unit, "px") == 0)) {
            encoder->pixelheight = number;
            encoder->percentheight = (-1);
        } else if (strcmp(value, "auto") == 0) {
            encoder->pixelheight = (-1);
            encoder->percentheight = (-1);
        } else {
            sixel_helper_set_additional_message(
                "cannot parse -h/--height option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->clipheight) {
            encoder->clipfirst = 1;
        }
        break;
    case SIXEL_OPTFLAG_RESAMPLING:  /* r */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_resampling,
            sizeof(g_option_choices_resampling) /
            sizeof(g_option_choices_resampling[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->method_for_resampling = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--resampling",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "specified desampling method is not supported.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_QUALITY:  /* q */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_quality,
            sizeof(g_option_choices_quality) /
            sizeof(g_option_choices_quality[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->quality_mode = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--quality",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "cannot parse quality option.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_LOOPMODE:  /* l */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_loopmode,
            sizeof(g_option_choices_loopmode) /
            sizeof(g_option_choices_loopmode[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->loop_mode = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--loop-control",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "cannot parse loop-control option.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_PALETTE_TYPE:  /* t */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_palette_type,
            sizeof(g_option_choices_palette_type) /
            sizeof(g_option_choices_palette_type[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->palette_type = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--palette-type",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "cannot parse palette type option.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_BGCOLOR:  /* B */
        /* parse --bgcolor option */
        if (encoder->bgcolor) {
            sixel_allocator_free(encoder->allocator, encoder->bgcolor);
            encoder->bgcolor = NULL;
        }
        status = sixel_parse_x_colorspec(&encoder->bgcolor,
                                         value,
                                         encoder->allocator);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "cannot parse bgcolor option.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_INSECURE:  /* k */
        encoder->finsecure = 1;
        break;
    case SIXEL_OPTFLAG_INVERT:  /* i */
        encoder->finvert = 1;
        break;
    case SIXEL_OPTFLAG_USE_MACRO:  /* u */
        encoder->fuse_macro = 1;
        break;
    case SIXEL_OPTFLAG_MACRO_NUMBER:  /* n */
        encoder->macro_number = atoi(value);
        if (encoder->macro_number < 0) {
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_IGNORE_DELAY:  /* g */
        encoder->fignore_delay = 1;
        break;
    case SIXEL_OPTFLAG_VERBOSE:  /* v */
        encoder->verbose = 1;
        sixel_helper_set_loader_trace(1);
        break;
    case SIXEL_OPTFLAG_LOADERS:  /* J */
        if (encoder->loader_order != NULL) {
            sixel_allocator_free(encoder->allocator,
                                 encoder->loader_order);
            encoder->loader_order = NULL;
        }
        if (value != NULL && *value != '\0') {
            encoder->loader_order = arg_strdup(value,
                                               encoder->allocator);
            if (encoder->loader_order == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_setopt: "
                    "sixel_allocator_malloc() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_STATIC:  /* S */
        encoder->fstatic = 1;
        break;
    case SIXEL_OPTFLAG_DRCS:  /* @ */
        encoder->fdrcs = 1;
        drcs_arg_delim = NULL;
        drcs_arg_charset = NULL;
        drcs_arg_second_delim = NULL;
        drcs_arg_path = NULL;
        drcs_arg_path_length = 0u;
        drcs_segment_length = 0u;
        drcs_mmv_value = 2;
        drcs_charset_value = 1L;
        drcs_charset_limit = 0u;
        if (value != NULL && *value != '\0') {
            drcs_arg_delim = strchr(value, ':');
            if (drcs_arg_delim == NULL) {
                drcs_segment_length = strlen(value);
                if (drcs_segment_length >= sizeof(drcs_segment)) {
                    sixel_helper_set_additional_message(
                        "DRCS mapping revision is too long.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
                memcpy(drcs_segment, value, drcs_segment_length);
                drcs_segment[drcs_segment_length] = '\0';
                errno = 0;
                endptr = NULL;
                drcs_mmv_value = (int)strtol(drcs_segment, &endptr, 10);
                if (errno != 0 || endptr == drcs_segment || *endptr != '\0') {
                    sixel_helper_set_additional_message(
                        "cannot parse DRCS option.");
                    status = SIXEL_BAD_ARGUMENT;
                    goto end;
                }
            } else {
                if (drcs_arg_delim != value) {
                    drcs_segment_length =
                        (size_t)(drcs_arg_delim - value);
                    if (drcs_segment_length >= sizeof(drcs_segment)) {
                        sixel_helper_set_additional_message(
                            "DRCS mapping revision is too long.");
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
                    }
                    memcpy(drcs_segment, value, drcs_segment_length);
                    drcs_segment[drcs_segment_length] = '\0';
                    errno = 0;
                    endptr = NULL;
                    drcs_mmv_value = (int)strtol(drcs_segment, &endptr, 10);
                    if (errno != 0 || endptr == drcs_segment || *endptr != '\0') {
                        sixel_helper_set_additional_message(
                            "cannot parse DRCS option.");
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
                    }
                }
                drcs_arg_charset = drcs_arg_delim + 1;
                drcs_arg_second_delim = strchr(drcs_arg_charset, ':');
                if (drcs_arg_second_delim != NULL) {
                    if (drcs_arg_second_delim != drcs_arg_charset) {
                        drcs_segment_length =
                            (size_t)(drcs_arg_second_delim - drcs_arg_charset);
                        if (drcs_segment_length >= sizeof(drcs_segment)) {
                            sixel_helper_set_additional_message(
                                "DRCS charset number is too long.");
                            status = SIXEL_BAD_ARGUMENT;
                            goto end;
                        }
                        memcpy(drcs_segment,
                               drcs_arg_charset,
                               drcs_segment_length);
                        drcs_segment[drcs_segment_length] = '\0';
                        errno = 0;
                        endptr = NULL;
                        drcs_charset_value = strtol(drcs_segment,
                                                    &endptr,
                                                    10);
                        if (errno != 0 || endptr == drcs_segment ||
                                *endptr != '\0') {
                            sixel_helper_set_additional_message(
                                "cannot parse DRCS charset number.");
                            status = SIXEL_BAD_ARGUMENT;
                            goto end;
                        }
                    }
                    drcs_arg_path = drcs_arg_second_delim + 1;
                    drcs_arg_path_length = strlen(drcs_arg_path);
                    if (drcs_arg_path_length == 0u) {
                        drcs_arg_path = NULL;
                    }
                } else if (*drcs_arg_charset != '\0') {
                    drcs_segment_length = strlen(drcs_arg_charset);
                    if (drcs_segment_length >= sizeof(drcs_segment)) {
                        sixel_helper_set_additional_message(
                            "DRCS charset number is too long.");
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
                    }
                    memcpy(drcs_segment,
                           drcs_arg_charset,
                           drcs_segment_length);
                    drcs_segment[drcs_segment_length] = '\0';
                    errno = 0;
                    endptr = NULL;
                    drcs_charset_value = strtol(drcs_segment,
                                                &endptr,
                                                10);
                    if (errno != 0 || endptr == drcs_segment ||
                            *endptr != '\0') {
                        sixel_helper_set_additional_message(
                            "cannot parse DRCS charset number.");
                        status = SIXEL_BAD_ARGUMENT;
                        goto end;
                    }
                }
            }
        }
        /*
         * Layout of the DRCS option value:
         *
         *    value = <mmv>:<charset_no>:<path>
         *          ^        ^                ^
         *          |        |                |
         *          |        |                +-- optional path that may reuse
         *          |        |                    STDOUT when set to "-" or drop
         *          |        |                    tiles when left blank
         *          |        +-- charset number (defaults to 1 when omitted)
         *          +-- mapping revision (defaults to 2 when omitted)
         */
        if (drcs_mmv_value < 0 || drcs_mmv_value > 2) {
            sixel_helper_set_additional_message(
                "unknown DRCS unicode mapping version.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (drcs_mmv_value == 0) {
            drcs_charset_limit = 126u;
        } else if (drcs_mmv_value == 1) {
            drcs_charset_limit = 63u;
        } else {
            drcs_charset_limit = 158u;
        }
        if (drcs_charset_value < 1 ||
            (unsigned long)drcs_charset_value > drcs_charset_limit) {
            sixel_helper_set_additional_message(
                "DRCS charset number is out of range.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        encoder->drcs_mmv = drcs_mmv_value;
        encoder->drcs_charset_no = (unsigned short)drcs_charset_value;
        if (encoder->tile_outfd >= 0
            && encoder->tile_outfd != encoder->outfd
            && encoder->tile_outfd != STDOUT_FILENO
            && encoder->tile_outfd != STDERR_FILENO) {
#if HAVE__CLOSE
            (void) _close(encoder->tile_outfd);
#else
            (void) close(encoder->tile_outfd);
#endif  /* HAVE__CLOSE */
        }
        encoder->tile_outfd = (-1);
        if (drcs_arg_path != NULL) {
            if (strcmp(drcs_arg_path, "-") == 0) {
                encoder->tile_outfd = STDOUT_FILENO;
            } else {
#if HAVE__OPEN
                encoder->tile_outfd = _open(drcs_arg_path,
                                            O_RDWR|O_CREAT|O_TRUNC,
                                            S_IRUSR|S_IWUSR);
#else
                encoder->tile_outfd = open(drcs_arg_path,
                                           O_RDWR|O_CREAT|O_TRUNC,
                                           S_IRUSR|S_IWUSR);
#endif  /* HAVE__OPEN */
                if (encoder->tile_outfd < 0) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_setopt: failed to open tile"
                        " output path.");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
            }
        }
        break;
    case SIXEL_OPTFLAG_PENETRATE:  /* P */
        encoder->penetrate_multiplexer = 1;
        break;
    case SIXEL_OPTFLAG_ENCODE_POLICY:  /* E */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_encode_policy,
            sizeof(g_option_choices_encode_policy) /
            sizeof(g_option_choices_encode_policy[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->encode_policy = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--encode-policy",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "cannot parse encode policy option.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_LUT_POLICY:  /* L */
        match_result = sixel_match_option_choice(
            value,
            g_option_choices_lut_policy,
            sizeof(g_option_choices_lut_policy) /
            sizeof(g_option_choices_lut_policy[0]),
            &match_value,
            match_detail,
            sizeof(match_detail));
        if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
            encoder->lut_policy = match_value;
        } else {
            if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                sixel_report_ambiguous_prefix("--lut-policy",
                                              value,
                                              match_detail,
                                              match_message,
                                              sizeof(match_message));
            } else {
                sixel_helper_set_additional_message(
                    "cannot parse lut policy option.");
            }
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (encoder->dither_cache != NULL) {
            sixel_dither_set_lut_policy(encoder->dither_cache,
                                        encoder->lut_policy);
        }
        break;
    case SIXEL_OPTFLAG_WORKING_COLORSPACE:  /* W */
        if (value == NULL) {
            sixel_helper_set_additional_message(
                "working-colorspace requires an argument.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        } else {
            len = strlen(value);

            if (len >= sizeof(lowered)) {
                sixel_helper_set_additional_message(
                    "specified working colorspace name is too long.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            for (i = 0; i < len; ++i) {
                lowered[i] = (char)tolower((unsigned char)value[i]);
            }
            lowered[len] = '\0';

            match_result = sixel_match_option_choice(
                lowered,
                g_option_choices_working_colorspace,
                sizeof(g_option_choices_working_colorspace) /
                sizeof(g_option_choices_working_colorspace[0]),
                &match_value,
                match_detail,
                sizeof(match_detail));
            if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
                encoder->working_colorspace = match_value;
            } else {
                if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                    sixel_report_ambiguous_prefix(
                        "--working-colorspace",
                        value,
                        match_detail,
                        match_message,
                        sizeof(match_message));
                } else {
                    sixel_helper_set_additional_message(
                        "unsupported working colorspace specified.");
                }
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_OUTPUT_COLORSPACE:  /* U */
        if (value == NULL) {
            sixel_helper_set_additional_message(
                "output-colorspace requires an argument.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        } else {
            len = strlen(value);

            if (len >= sizeof(lowered)) {
                sixel_helper_set_additional_message(
                    "specified output colorspace name is too long.");
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
            for (i = 0; i < len; ++i) {
                lowered[i] = (char)tolower((unsigned char)value[i]);
            }
            lowered[len] = '\0';

            match_result = sixel_match_option_choice(
                lowered,
                g_option_choices_output_colorspace,
                sizeof(g_option_choices_output_colorspace) /
                sizeof(g_option_choices_output_colorspace[0]),
                &match_value,
                match_detail,
                sizeof(match_detail));
            if (match_result == SIXEL_OPTION_CHOICE_MATCH) {
                encoder->output_colorspace = match_value;
            } else {
                if (match_result == SIXEL_OPTION_CHOICE_AMBIGUOUS) {
                    sixel_report_ambiguous_prefix(
                        "--output-colorspace",
                        value,
                        match_detail,
                        match_message,
                        sizeof(match_message));
                } else {
                    sixel_helper_set_additional_message(
                        "unsupported output colorspace specified.");
                }
                status = SIXEL_BAD_ARGUMENT;
                goto end;
            }
        }
        break;
    case SIXEL_OPTFLAG_ORMODE:  /* O */
        encoder->ormode = 1;
        break;
    case SIXEL_OPTFLAG_COMPLEXION_SCORE:  /* C */
        encoder->complexion = atoi(value);
        if (encoder->complexion < 1) {
            sixel_helper_set_additional_message(
                "complexion parameter must be 1 or more.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        break;
    case SIXEL_OPTFLAG_PIPE_MODE:  /* D */
        encoder->pipe_mode = 1;
        break;
    case '?':  /* unknown option */
    default:
        /* exit if unknown options are specified */
        sixel_helper_set_additional_message(
            "unknown option is specified.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    /* detects arguments conflictions */
    if (encoder->reqcolors != (-1)) {
        switch (encoder->color_option) {
        case SIXEL_COLOR_OPTION_MAPFILE:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -m, --mapfile.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_MONOCHROME:
            sixel_helper_set_additional_message(
                "option -e, --monochrome conflicts with -p, --colors.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_HIGHCOLOR:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -I, --high-color.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        case SIXEL_COLOR_OPTION_BUILTIN:
            sixel_helper_set_additional_message(
                "option -p, --colors conflicts with -b, --builtin-palette.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        default:
            break;
        }
    }

    /* 8bit output option(-8) conflicts width GNU Screen integration(-P) */
    if (encoder->f8bit && encoder->penetrate_multiplexer) {
        sixel_helper_set_additional_message(
            "option -8 --8bit-mode conflicts"
            " with -P, --penetrate.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (opt_copy != NULL) {
        sixel_allocator_free(encoder->allocator, opt_copy);
    }
    sixel_encoder_unref(encoder);

    return status;
}


/* called when image loader component load a image frame */
static SIXELSTATUS
load_image_callback(sixel_frame_t *frame, void *data)
{
    sixel_encoder_t *encoder;

    encoder = (sixel_encoder_t *)data;
    if (encoder->capture_source && encoder->capture_source_frame == NULL) {
        sixel_frame_ref(frame);
        encoder->capture_source_frame = frame;
    }

    return sixel_encoder_encode_frame(encoder, frame, NULL);
}

/*
 * Build a tee for encoded-assessment output:
 *
 *     +-------------+     +-------------------+     +------------+
 *     | encoder FD  | --> | temporary SIXEL   | --> | tee sink   |
 *     +-------------+     +-------------------+     +------------+
 *
 * The tee sink can be stdout or a user-provided file such as /dev/null.
 */
static SIXELSTATUS
copy_file_to_stream(char const *path,
                    FILE *stream,
                    sixel_assessment_t *assessment)
{
    FILE *source;
    unsigned char buffer[4096];
    size_t nread;
    size_t nwritten;
    double started_at;
    double finished_at;
    double duration;

    source = NULL;
    nread = 0;
    nwritten = 0;
    started_at = 0.0;
    finished_at = 0.0;
    duration = 0.0;

    source = sixel_compat_fopen(path, "rb");
    if (source == NULL) {
        sixel_helper_set_additional_message(
            "copy_file_to_stream: failed to open assessment staging file.");
        return SIXEL_LIBC_ERROR;
    }

    for (;;) {
        nread = fread(buffer, 1, sizeof(buffer), source);
        if (nread == 0) {
            if (ferror(source)) {
                sixel_helper_set_additional_message(
                    "copy_file_to_stream: failed while reading assessment staging file.");
                (void) fclose(source);
                return SIXEL_LIBC_ERROR;
            }
            break;
        }
        if (assessment != NULL) {
            started_at = sixel_assessment_timer_now();
        }
        nwritten = fwrite(buffer, 1, nread, stream);
        if (nwritten != nread) {
            sixel_helper_set_additional_message(
                "img2sixel: failed while copying assessment staging file.");
            (void) fclose(source);
            return SIXEL_LIBC_ERROR;
        }
        if (assessment != NULL) {
            finished_at = sixel_assessment_timer_now();
            duration = finished_at - started_at;
            if (duration < 0.0) {
                duration = 0.0;
            }
            sixel_assessment_record_output_write(assessment,
                                                 nwritten,
                                                 duration);
        }
    }

    if (fclose(source) != 0) {
        sixel_helper_set_additional_message(
            "img2sixel: failed to close assessment staging file.");
        return SIXEL_LIBC_ERROR;
    }

    return SIXEL_OK;
}

typedef struct assessment_json_sink {
    FILE *stream;
    int failed;
} assessment_json_sink_t;

static void
assessment_json_callback(char const *chunk,
                         size_t length,
                         void *user_data)
{
    assessment_json_sink_t *sink;

    sink = (assessment_json_sink_t *)user_data;
    if (sink == NULL || sink->stream == NULL) {
        return;
    }
    if (sink->failed) {
        return;
    }
    if (fwrite(chunk, 1, length, sink->stream) != length) {
        sink->failed = 1;
    }
}

static char *
create_temp_template_with_prefix(sixel_allocator_t *allocator,
                                 char const *prefix,
                                 size_t *capacity_out)
{
    char const *tmpdir;
    size_t tmpdir_len;
    size_t prefix_len;
    size_t suffix_len;
    size_t template_len;
    char *template_path;
    int needs_separator;
    size_t maximum_tmpdir_len;

    tmpdir = sixel_compat_getenv("TMPDIR");
#if defined(_WIN32)
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = sixel_compat_getenv("TEMP");
    }
    if (tmpdir == NULL || tmpdir[0] == '\0') {
        tmpdir = sixel_compat_getenv("TMP");
    }
#endif
    if (tmpdir == NULL || tmpdir[0] == '\0') {
#if defined(_WIN32)
        tmpdir = ".";
#else
        tmpdir = "/tmp";
#endif
    }

    tmpdir_len = strlen(tmpdir);
    prefix_len = 0u;
    suffix_len = 0u;
    if (prefix == NULL) {
        return NULL;
    }

    prefix_len = strlen(prefix);
    suffix_len = prefix_len + strlen("-XXXXXX");
    maximum_tmpdir_len = (size_t)INT_MAX;

    if (maximum_tmpdir_len <= suffix_len + 2) {
        return NULL;
    }
    if (tmpdir_len > maximum_tmpdir_len - (suffix_len + 2)) {
        return NULL;
    }
    needs_separator = 1;
    if (tmpdir_len > 0) {
        if (tmpdir[tmpdir_len - 1] == '/' || tmpdir[tmpdir_len - 1] == '\\') {
            needs_separator = 0;
        }
    }

    template_len = tmpdir_len + suffix_len + 2;
    template_path = (char *)sixel_allocator_malloc(allocator, template_len);
    if (template_path == NULL) {
        return NULL;
    }

    if (needs_separator) {
#if defined(_WIN32)
        (void) snprintf(template_path, template_len,
                        "%s\\%s-XXXXXX", tmpdir, prefix);
#else
        (void) snprintf(template_path, template_len,
                        "%s/%s-XXXXXX", tmpdir, prefix);
#endif
    } else {
        (void) snprintf(template_path, template_len,
                        "%s%s-XXXXXX", tmpdir, prefix);
    }

    if (capacity_out != NULL) {
        *capacity_out = template_len;
    }

    return template_path;
}


static char *
create_temp_template(sixel_allocator_t *allocator,
                     size_t *capacity_out)
{
    return create_temp_template_with_prefix(allocator,
                                            "img2sixel",
                                            capacity_out);
}


static void
clipboard_select_format(char *dest,
                        size_t dest_size,
                        char const *format,
                        char const *fallback)
{
    char const *source;
    size_t limit;

    if (dest == NULL || dest_size == 0u) {
        return;
    }

    source = fallback;
    if (format != NULL && format[0] != '\0') {
        source = format;
    }

    limit = dest_size - 1u;
    if (limit == 0u) {
        dest[0] = '\0';
        return;
    }

    (void)snprintf(dest, dest_size, "%.*s", (int)limit, source);
}


static SIXELSTATUS
clipboard_create_spool(sixel_allocator_t *allocator,
                       char const *prefix,
                       char **path_out,
                       int *fd_out)
{
    SIXELSTATUS status;
    char *template_path;
    size_t template_capacity;
    int open_flags;
    int fd;
    char *tmpname_result;

    status = SIXEL_FALSE;
    template_path = NULL;
    template_capacity = 0u;
    open_flags = 0;
    fd = (-1);
    tmpname_result = NULL;

    template_path = create_temp_template_with_prefix(allocator,
                                                     prefix,
                                                     &template_capacity);
    if (template_path == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to allocate spool template.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (sixel_compat_mktemp(template_path, template_capacity) != 0) {
        /* Fall back to tmpnam() when mktemp variants are unavailable. */
        tmpname_result = tmpnam(template_path);
        if (tmpname_result == NULL) {
            sixel_helper_set_additional_message(
                "clipboard: failed to reserve spool template.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
        template_capacity = strlen(template_path) + 1u;
    }

    open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_EXCL)
    open_flags |= O_EXCL;
#endif
    fd = sixel_compat_open(template_path, open_flags, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        sixel_helper_set_additional_message(
            "clipboard: failed to open spool file.");
        status = SIXEL_LIBC_ERROR;
        goto end;
    }

    *path_out = template_path;
    if (fd_out != NULL) {
        *fd_out = fd;
        fd = (-1);
    }

    template_path = NULL;
    status = SIXEL_OK;

end:
    if (fd >= 0) {
        (void)sixel_compat_close(fd);
    }
    if (template_path != NULL) {
        sixel_allocator_free(allocator, template_path);
    }

    return status;
}


static SIXELSTATUS
clipboard_write_file(char const *path,
                     unsigned char const *data,
                     size_t size)
{
    FILE *stream;
    size_t written;

    if (path == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: spool path is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    stream = sixel_compat_fopen(path, "wb");
    if (stream == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to open spool file for write.");
        return SIXEL_LIBC_ERROR;
    }

    written = 0u;
    if (size > 0u && data != NULL) {
        written = fwrite(data, 1u, size, stream);
        if (written != size) {
            (void)fclose(stream);
            sixel_helper_set_additional_message(
                "clipboard: failed to write spool payload.");
            return SIXEL_LIBC_ERROR;
        }
    }

    if (fclose(stream) != 0) {
        sixel_helper_set_additional_message(
            "clipboard: failed to close spool file after write.");
        return SIXEL_LIBC_ERROR;
    }

    return SIXEL_OK;
}


static SIXELSTATUS
clipboard_read_file(char const *path,
                    unsigned char **data,
                    size_t *size)
{
    FILE *stream;
    long seek_result;
    long file_size;
    unsigned char *buffer;
    size_t read_size;

    if (data == NULL || size == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: read buffer pointers are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *data = NULL;
    *size = 0u;

    if (path == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: spool path is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    stream = sixel_compat_fopen(path, "rb");
    if (stream == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: failed to open spool file for read.");
        return SIXEL_LIBC_ERROR;
    }

    seek_result = fseek(stream, 0L, SEEK_END);
    if (seek_result != 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to seek spool file.");
        return SIXEL_LIBC_ERROR;
    }

    file_size = ftell(stream);
    if (file_size < 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to determine spool size.");
        return SIXEL_LIBC_ERROR;
    }

    seek_result = fseek(stream, 0L, SEEK_SET);
    if (seek_result != 0) {
        (void)fclose(stream);
        sixel_helper_set_additional_message(
            "clipboard: failed to rewind spool file.");
        return SIXEL_LIBC_ERROR;
    }

    if (file_size == 0) {
        buffer = NULL;
        read_size = 0u;
    } else {
        buffer = (unsigned char *)malloc((size_t)file_size);
        if (buffer == NULL) {
            (void)fclose(stream);
            sixel_helper_set_additional_message(
                "clipboard: malloc() failed for spool payload.");
            return SIXEL_BAD_ALLOCATION;
        }
        read_size = fread(buffer, 1u, (size_t)file_size, stream);
        if (read_size != (size_t)file_size) {
            free(buffer);
            (void)fclose(stream);
            sixel_helper_set_additional_message(
                "clipboard: failed to read spool payload.");
            return SIXEL_LIBC_ERROR;
        }
    }

    if (fclose(stream) != 0) {
        if (buffer != NULL) {
            free(buffer);
        }
        sixel_helper_set_additional_message(
            "clipboard: failed to close spool file after read.");
        return SIXEL_LIBC_ERROR;
    }

    *data = buffer;
    *size = read_size;

    return SIXEL_OK;
}


static SIXELSTATUS
write_png_from_sixel(char const *sixel_path, char const *output_path)
{
    SIXELSTATUS status;
    sixel_decoder_t *decoder;

    status = SIXEL_FALSE;
    decoder = NULL;

    status = sixel_decoder_new(&decoder, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_INPUT, sixel_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decoder_setopt(decoder, SIXEL_OPTFLAG_OUTPUT, output_path);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_decoder_decode(decoder);

end:
    sixel_decoder_unref(decoder);

    return status;
}


/* load source data from specified file and encode it to SIXEL format
 * output to encoder->outfd */
SIXELAPI SIXELSTATUS
sixel_encoder_encode(
    sixel_encoder_t *encoder,   /* encoder object */
    char const      *filename)  /* input filename */
{
    SIXELSTATUS status = SIXEL_FALSE;
    SIXELSTATUS palette_status = SIXEL_OK;
    int fuse_palette = 1;
    sixel_loader_t *loader = NULL;
    sixel_allocator_t *assessment_allocator = NULL;
    sixel_frame_t *assessment_source_frame = NULL;
    sixel_frame_t *assessment_target_frame = NULL;
    sixel_frame_t *assessment_expanded_frame = NULL;
    unsigned int assessment_section_mask =
        encoder->assessment_sections & SIXEL_ASSESSMENT_SECTION_MASK;
    int assessment_need_source_capture = 0;
    int assessment_need_quantized_capture = 0;
    int assessment_need_quality = 0;
    int assessment_quality_quantized = 0;
    assessment_json_sink_t assessment_sink;
    FILE *assessment_json_file = NULL;
    FILE *assessment_forward_stream = NULL;
    int assessment_json_owned = 0;
    char *assessment_temp_path = NULL;
    size_t assessment_temp_capacity = 0u;
    char *assessment_tmpnam_result = NULL;
    sixel_assessment_spool_mode_t assessment_spool_mode
        = SIXEL_ASSESSMENT_SPOOL_MODE_NONE;
    char *assessment_forward_path = NULL;
    size_t assessment_output_bytes;
#if HAVE_SYS_STAT_H
    struct stat assessment_stat;
    int assessment_stat_result;
    char const *assessment_size_path = NULL;
#endif
    char const *png_final_path = NULL;
    char *png_temp_path = NULL;
    size_t png_temp_capacity = 0u;
    char *png_tmpnam_result = NULL;
    int png_open_flags = 0;
    int spool_required;
    sixel_clipboard_spec_t clipboard_spec;
    char clipboard_input_format[32];
    char *clipboard_input_path;
    unsigned char *clipboard_blob;
    size_t clipboard_blob_size;
    SIXELSTATUS clipboard_status;
    char const *effective_filename;

    clipboard_input_format[0] = '\0';
    clipboard_input_path = NULL;
    clipboard_blob = NULL;
    clipboard_blob_size = 0u;
    clipboard_status = SIXEL_OK;
    effective_filename = filename;

    clipboard_spec.is_clipboard = 0;
    clipboard_spec.format[0] = '\0';
    if (effective_filename != NULL
            && sixel_clipboard_parse_spec(effective_filename,
                                          &clipboard_spec)
            && clipboard_spec.is_clipboard) {
        clipboard_select_format(clipboard_input_format,
                                sizeof(clipboard_input_format),
                                clipboard_spec.format,
                                "sixel");
        clipboard_status = sixel_clipboard_read(
            clipboard_input_format,
            &clipboard_blob,
            &clipboard_blob_size,
            encoder->allocator);
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        clipboard_status = clipboard_create_spool(
            encoder->allocator,
            "clipboard-in",
            &clipboard_input_path,
            NULL);
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        clipboard_status = clipboard_write_file(
            clipboard_input_path,
            clipboard_blob,
            clipboard_blob_size);
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        if (clipboard_blob != NULL) {
            free(clipboard_blob);
            clipboard_blob = NULL;
        }
        effective_filename = clipboard_input_path;
    }

    if (assessment_section_mask != SIXEL_ASSESSMENT_SECTION_NONE) {
        status = sixel_allocator_new(&assessment_allocator,
                                     malloc,
                                     calloc,
                                     realloc,
                                     free);
        if (SIXEL_FAILED(status) || assessment_allocator == NULL) {
            goto end;
        }
        status = sixel_assessment_new(&encoder->assessment_observer,
                                       assessment_allocator);
        if (SIXEL_FAILED(status) || encoder->assessment_observer == NULL) {
            goto end;
        }
        sixel_assessment_select_sections(encoder->assessment_observer,
                                         encoder->assessment_sections);
        sixel_assessment_attach_encoder(encoder->assessment_observer,
                                        encoder);
        assessment_need_quality =
            (assessment_section_mask & SIXEL_ASSESSMENT_SECTION_QUALITY) != 0u;
        assessment_quality_quantized =
            (encoder->assessment_sections & SIXEL_ASSESSMENT_VIEW_QUANTIZED) != 0u;
        assessment_need_quantized_capture =
            ((assessment_section_mask &
              (SIXEL_ASSESSMENT_SECTION_BASIC |
               SIXEL_ASSESSMENT_SECTION_SIZE)) != 0u) ||
            assessment_quality_quantized;
        assessment_need_source_capture =
            (assessment_section_mask &
             (SIXEL_ASSESSMENT_SECTION_BASIC |
              SIXEL_ASSESSMENT_SECTION_PERFORMANCE |
              SIXEL_ASSESSMENT_SECTION_SIZE |
              SIXEL_ASSESSMENT_SECTION_QUALITY)) != 0u;
        if (assessment_need_quality && !assessment_quality_quantized &&
                encoder->output_is_png) {
            sixel_helper_set_additional_message(
                "sixel_encoder_setopt: encoded quality assessment requires SIXEL output.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        status = sixel_encoder_enable_source_capture(encoder,
                                                     assessment_need_source_capture);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_STATIC, NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        if (assessment_need_quantized_capture) {
            status = sixel_encoder_enable_quantized_capture(encoder, 1);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_NONE;
        spool_required = 0;
        if (assessment_need_quality && !assessment_quality_quantized) {
            if (encoder->sixel_output_path == NULL) {
                assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT;
                spool_required = 1;
            } else if (strcmp(encoder->sixel_output_path, "-") == 0) {
                assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT;
                spool_required = 1;
                free(encoder->sixel_output_path);
                encoder->sixel_output_path = NULL;
            } else if (is_dev_null_path(encoder->sixel_output_path)) {
                assessment_spool_mode = SIXEL_ASSESSMENT_SPOOL_MODE_PATH;
                spool_required = 1;
                assessment_forward_path = encoder->sixel_output_path;
                encoder->sixel_output_path = NULL;
            }
        }
        if (spool_required) {
            assessment_temp_capacity = 0u;
            assessment_tmpnam_result = NULL;
            assessment_temp_path = create_temp_template(encoder->allocator,
                                                        &assessment_temp_capacity);
            if (assessment_temp_path == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_encode: sixel_allocator_malloc() "
                    "failed for assessment staging path.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            if (sixel_compat_mktemp(assessment_temp_path,
                                    assessment_temp_capacity) != 0) {
                /* Fall back to tmpnam() when mktemp variants are unavailable. */
                assessment_tmpnam_result = tmpnam(assessment_temp_path);
                if (assessment_tmpnam_result == NULL) {
                    sixel_helper_set_additional_message(
                        "sixel_encoder_encode: mktemp() failed for assessment staging file.");
                    status = SIXEL_RUNTIME_ERROR;
                    goto end;
                }
                assessment_temp_capacity = strlen(assessment_temp_path) + 1u;
            }
            status = sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_OUTFILE,
                                          assessment_temp_path);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            encoder->sixel_output_path = (char *)sixel_allocator_malloc(
                encoder->allocator, strlen(assessment_temp_path) + 1);
            if (encoder->sixel_output_path == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_encode: malloc() failed for assessment staging name.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            (void)sixel_compat_strcpy(encoder->sixel_output_path,
                                      strlen(assessment_temp_path) + 1,
                                      assessment_temp_path);
        }

    }

    if (encoder->output_is_png) {
        png_temp_capacity = 0u;
        png_tmpnam_result = NULL;
        png_temp_path = create_temp_template(encoder->allocator,
                                             &png_temp_capacity);
        if (png_temp_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: malloc() failed for PNG staging path.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
        if (sixel_compat_mktemp(png_temp_path, png_temp_capacity) != 0) {
            /* Fall back to tmpnam() when mktemp variants are unavailable. */
            png_tmpnam_result = tmpnam(png_temp_path);
            if (png_tmpnam_result == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_encode: mktemp() failed for PNG staging file.");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            png_temp_capacity = strlen(png_temp_path) + 1u;
        }
        if (encoder->outfd >= 0 && encoder->outfd != STDOUT_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
        }
        png_open_flags = O_RDWR | O_CREAT | O_TRUNC;
#if defined(O_EXCL)
        png_open_flags |= O_EXCL;
#endif
        encoder->outfd = sixel_compat_open(png_temp_path,
                                           png_open_flags,
                                           S_IRUSR | S_IWUSR);
        if (encoder->outfd < 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: failed to create the PNG target file.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
    }

    if (encoder == NULL) {
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
        encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
        if (encoder == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: sixel_encoder_create() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto end;
        }
    } else {
        sixel_encoder_ref(encoder);
    }

    if (encoder->assessment_observer != NULL) {
        sixel_assessment_stage_transition(
            encoder->assessment_observer,
            SIXEL_ASSESSMENT_STAGE_IMAGE_CHUNK);
    }
    encoder->last_loader_name[0] = '\0';
    encoder->last_source_path[0] = '\0';
    encoder->last_input_bytes = 0u;

    /* if required color is not set, set the max value */
    if (encoder->reqcolors == (-1)) {
        encoder->reqcolors = SIXEL_PALETTE_MAX;
    }

    if (encoder->capture_source && encoder->capture_source_frame != NULL) {
        sixel_frame_unref(encoder->capture_source_frame);
        encoder->capture_source_frame = NULL;
    }

    /* if required color is less then 2, set the min value */
    if (encoder->reqcolors < 2) {
        encoder->reqcolors = SIXEL_PALETTE_MIN;
    }

    /* if color space option is not set, choose RGB color space */
    if (encoder->palette_type == SIXEL_PALETTETYPE_AUTO) {
        encoder->palette_type = SIXEL_PALETTETYPE_RGB;
    }

    /* if color option is not default value, prohibit to read
       the file as a paletted image */
    if (encoder->color_option != SIXEL_COLOR_OPTION_DEFAULT) {
        fuse_palette = 0;
    }

    /* if scaling options are set, prohibit to read the file as
       a paletted image */
    if (encoder->percentwidth > 0 ||
        encoder->percentheight > 0 ||
        encoder->pixelwidth > 0 ||
        encoder->pixelheight > 0) {
        fuse_palette = 0;
    }

reload:

    sixel_helper_set_loader_trace(encoder->verbose);
    sixel_helper_set_thumbnail_size_hint(
        sixel_encoder_thumbnail_hint(encoder));

    status = sixel_loader_new(&loader, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &encoder->fstatic);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &encoder->reqcolors);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_BGCOLOR,
                                 encoder->bgcolor);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &encoder->loop_mode);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &encoder->finsecure);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CANCEL_FLAG,
                                 encoder->cancel_flag);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOADER_ORDER,
                                 encoder->loader_order);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 encoder);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    /*
     * Wire the optional assessment observer into the loader.
     *
     * The observer travels separately from the callback context so mapfile
     * palette probes and other callbacks can keep using arbitrary structs.
     */
    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_ASSESSMENT,
                                 encoder->assessment_observer);
    if (SIXEL_FAILED(status)) {
        goto load_end;
    }

    status = sixel_loader_load_file(loader,
                                    effective_filename,
                                    load_image_callback);
    if (status != SIXEL_OK) {
        goto load_end;
    }
    encoder->last_input_bytes = sixel_loader_get_last_input_bytes(loader);
    if (sixel_loader_get_last_success_name(loader) != NULL) {
        (void)snprintf(encoder->last_loader_name,
                       sizeof(encoder->last_loader_name),
                       "%s",
                       sixel_loader_get_last_success_name(loader));
    } else {
        encoder->last_loader_name[0] = '\0';
    }
    if (sixel_loader_get_last_source_path(loader) != NULL) {
        (void)snprintf(encoder->last_source_path,
                       sizeof(encoder->last_source_path),
                       "%s",
                       sixel_loader_get_last_source_path(loader));
    } else {
        encoder->last_source_path[0] = '\0';
    }
    if (encoder->assessment_observer != NULL) {
        sixel_assessment_record_loader(encoder->assessment_observer,
                                       encoder->last_source_path,
                                       encoder->last_loader_name,
                                       encoder->last_input_bytes);
    }

load_end:
    sixel_loader_unref(loader);
    loader = NULL;

    if (status != SIXEL_OK) {
        goto end;
    }

    palette_status = sixel_encoder_emit_palette_output(encoder);
    if (SIXEL_FAILED(palette_status)) {
        status = palette_status;
        goto end;
    }

    if (encoder->pipe_mode) {
#if HAVE_CLEARERR
        clearerr(stdin);
#endif  /* HAVE_FSEEK */
        while (encoder->cancel_flag && !*encoder->cancel_flag) {
            status = sixel_tty_wait_stdin(1000000);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            if (status != SIXEL_OK) {
                break;
            }
        }
        if (!encoder->cancel_flag || !*encoder->cancel_flag) {
            goto reload;
        }
    }

    if (encoder->assessment_observer) {
        if (assessment_allocator == NULL || encoder->assessment_observer == NULL) {
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
        if (assessment_need_source_capture) {
            status = sixel_encoder_copy_source_frame(encoder,
                                                     &assessment_source_frame);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            sixel_assessment_record_source_frame(encoder->assessment_observer,
                                                 assessment_source_frame);
        }
        if (assessment_need_quality) {
            if (assessment_quality_quantized) {
                status = sixel_encoder_copy_quantized_frame(
                    encoder, assessment_allocator, &assessment_target_frame);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                status = sixel_assessment_expand_quantized_frame(
                    assessment_target_frame,
                    assessment_allocator,
                    &assessment_expanded_frame);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
                sixel_frame_unref(assessment_target_frame);
                assessment_target_frame = assessment_expanded_frame;
                assessment_expanded_frame = NULL;
                sixel_assessment_record_quantized_capture(
                    encoder->assessment_observer, encoder);
            } else {
                status = sixel_assessment_load_single_frame(
                    encoder->sixel_output_path,
                    assessment_allocator,
                    &assessment_target_frame);
                if (SIXEL_FAILED(status)) {
                    goto end;
                }
            }
            if (!assessment_quality_quantized &&
                    assessment_need_quantized_capture) {
                sixel_assessment_record_quantized_capture(
                    encoder->assessment_observer, encoder);
            }
        } else if (assessment_need_quantized_capture) {
            sixel_assessment_record_quantized_capture(
                encoder->assessment_observer, encoder);
        }
        if (encoder->assessment_observer != NULL &&
                assessment_spool_mode != SIXEL_ASSESSMENT_SPOOL_MODE_NONE) {
            sixel_assessment_stage_transition(
                encoder->assessment_observer,
                SIXEL_ASSESSMENT_STAGE_OUTPUT);
        }
        if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT) {
            status = copy_file_to_stream(assessment_temp_path,
                                         stdout,
                                         encoder->assessment_observer);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        } else if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_PATH) {
            if (assessment_forward_path == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_encode: missing assessment spool target.");
                status = SIXEL_RUNTIME_ERROR;
                goto end;
            }
            assessment_forward_stream = sixel_compat_fopen(
                assessment_forward_path,
                "wb");
            if (assessment_forward_stream == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_encode: failed to open assessment spool sink.");
                status = SIXEL_LIBC_ERROR;
                goto end;
            }
            status = copy_file_to_stream(assessment_temp_path,
                                         assessment_forward_stream,
                                         encoder->assessment_observer);
            if (fclose(assessment_forward_stream) != 0) {
                if (SIXEL_SUCCEEDED(status)) {
                    sixel_helper_set_additional_message(
                        "img2sixel: failed to close assessment spool sink.");
                    status = SIXEL_LIBC_ERROR;
                }
            }
            assessment_forward_stream = NULL;
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        if (encoder->assessment_observer != NULL &&
                assessment_spool_mode != SIXEL_ASSESSMENT_SPOOL_MODE_NONE) {
            sixel_assessment_stage_finish(encoder->assessment_observer);
        }
#if HAVE_SYS_STAT_H
        assessment_output_bytes = 0u;
        assessment_size_path = NULL;
        if (assessment_need_quality && !assessment_quality_quantized) {
            if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT ||
                    assessment_spool_mode ==
                        SIXEL_ASSESSMENT_SPOOL_MODE_PATH) {
                assessment_size_path = assessment_temp_path;
            } else if (encoder->sixel_output_path != NULL &&
                    strcmp(encoder->sixel_output_path, "-") != 0) {
                assessment_size_path = encoder->sixel_output_path;
            }
        } else {
            if (encoder->sixel_output_path != NULL &&
                    strcmp(encoder->sixel_output_path, "-") != 0) {
                assessment_size_path = encoder->sixel_output_path;
            } else if (assessment_spool_mode ==
                    SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT ||
                    assessment_spool_mode ==
                        SIXEL_ASSESSMENT_SPOOL_MODE_PATH) {
                assessment_size_path = assessment_temp_path;
            }
        }
        if (assessment_size_path != NULL) {
            assessment_stat_result = stat(assessment_size_path,
                                          &assessment_stat);
            if (assessment_stat_result == 0 &&
                    assessment_stat.st_size >= 0) {
                assessment_output_bytes =
                    (size_t)assessment_stat.st_size;
            }
        }
#else
        assessment_output_bytes = 0u;
#endif
        sixel_assessment_record_output_size(encoder->assessment_observer,
                                            assessment_output_bytes);
        if (assessment_need_quality) {
            status = sixel_assessment_analyze(encoder->assessment_observer,
                                              assessment_source_frame,
                                              assessment_target_frame);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
        }
        sixel_assessment_stage_finish(encoder->assessment_observer);
        if (encoder->assessment_json_path != NULL &&
                strcmp(encoder->assessment_json_path, "-") != 0) {
            assessment_json_file = sixel_compat_fopen(
                encoder->assessment_json_path,
                "wb");
            if (assessment_json_file == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_encode: failed to open assessment JSON file.");
                status = SIXEL_LIBC_ERROR;
                goto end;
            }
            assessment_json_owned = 1;
            assessment_sink.stream = assessment_json_file;
        } else {
            assessment_sink.stream = stdout;
        }
        assessment_sink.failed = 0;
        status = sixel_assessment_get_json(encoder->assessment_observer,
                                           encoder->assessment_sections,
                                           assessment_json_callback,
                                           &assessment_sink);
        if (SIXEL_FAILED(status) || assessment_sink.failed) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: failed to emit assessment JSON.");
            goto end;
        }
    } else if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_STDOUT) {
        status = copy_file_to_stream(assessment_temp_path,
                                     stdout,
                                     encoder->assessment_observer);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else if (assessment_spool_mode == SIXEL_ASSESSMENT_SPOOL_MODE_PATH) {
        if (assessment_forward_path == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: missing assessment spool target.");
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
        assessment_forward_stream = sixel_compat_fopen(
            assessment_forward_path,
            "wb");
        if (assessment_forward_stream == NULL) {
            sixel_helper_set_additional_message(
                "img2sixel: failed to open assessment spool sink.");
            status = SIXEL_LIBC_ERROR;
            goto end;
        }
        status = copy_file_to_stream(assessment_temp_path,
                                     assessment_forward_stream,
                                     encoder->assessment_observer);
        if (fclose(assessment_forward_stream) != 0) {
            if (SIXEL_SUCCEEDED(status)) {
                sixel_helper_set_additional_message(
                    "img2sixel: failed to close assessment spool sink.");
                status = SIXEL_LIBC_ERROR;
            }
        }
        assessment_forward_stream = NULL;
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (encoder->output_is_png) {
        png_final_path = encoder->output_png_to_stdout ? "-" : encoder->png_output_path;
        if (! encoder->output_png_to_stdout && png_final_path == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_encode: missing PNG output path.");
            status = SIXEL_RUNTIME_ERROR;
            goto end;
        }
        status = write_png_from_sixel(png_temp_path, png_final_path);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    if (encoder->clipboard_output_active
            && encoder->clipboard_output_path != NULL) {
        unsigned char *clipboard_output_data;
        size_t clipboard_output_size;

        clipboard_output_data = NULL;
        clipboard_output_size = 0u;

        if (encoder->outfd
                && encoder->outfd != STDOUT_FILENO
                && encoder->outfd != STDERR_FILENO) {
            (void)sixel_compat_close(encoder->outfd);
            encoder->outfd = STDOUT_FILENO;
        }

        clipboard_status = clipboard_read_file(
            encoder->clipboard_output_path,
            &clipboard_output_data,
            &clipboard_output_size);
        if (SIXEL_SUCCEEDED(clipboard_status)) {
            clipboard_status = sixel_clipboard_write(
                encoder->clipboard_output_format,
                clipboard_output_data,
                clipboard_output_size);
        }
        if (clipboard_output_data != NULL) {
            free(clipboard_output_data);
        }
        if (SIXEL_FAILED(clipboard_status)) {
            status = clipboard_status;
            goto end;
        }
        (void)sixel_compat_unlink(encoder->clipboard_output_path);
        sixel_allocator_free(encoder->allocator,
                             encoder->clipboard_output_path);
        encoder->clipboard_output_path = NULL;
        encoder->sixel_output_path = NULL;
        encoder->clipboard_output_active = 0;
        encoder->clipboard_output_format[0] = '\0';
    }

    /* the status may not be SIXEL_OK */

end:
    if (png_temp_path != NULL) {
        (void)sixel_compat_unlink(png_temp_path);
    }
    sixel_allocator_free(encoder->allocator, png_temp_path);
    if (clipboard_input_path != NULL) {
        (void)sixel_compat_unlink(clipboard_input_path);
        sixel_allocator_free(encoder->allocator, clipboard_input_path);
    }
    if (clipboard_blob != NULL) {
        free(clipboard_blob);
    }
    if (encoder->clipboard_output_path != NULL) {
        (void)sixel_compat_unlink(encoder->clipboard_output_path);
        sixel_allocator_free(encoder->allocator,
                             encoder->clipboard_output_path);
        encoder->clipboard_output_path = NULL;
        encoder->sixel_output_path = NULL;
        encoder->clipboard_output_active = 0;
        encoder->clipboard_output_format[0] = '\0';
    }
    sixel_allocator_free(encoder->allocator, encoder->png_output_path);
    encoder->png_output_path = NULL;
    if (assessment_forward_stream != NULL) {
        (void) fclose(assessment_forward_stream);
    }
    if (assessment_temp_path != NULL &&
            assessment_spool_mode != SIXEL_ASSESSMENT_SPOOL_MODE_NONE) {
        (void)sixel_compat_unlink(assessment_temp_path);
    }
    sixel_allocator_free(encoder->allocator, assessment_temp_path);
    sixel_allocator_free(encoder->allocator, assessment_forward_path);
    if (assessment_json_owned && assessment_json_file != NULL) {
        (void) fclose(assessment_json_file);
    }
    if (assessment_target_frame != NULL) {
        sixel_frame_unref(assessment_target_frame);
    }
    if (assessment_expanded_frame != NULL) {
        sixel_frame_unref(assessment_expanded_frame);
    }
    if (assessment_source_frame != NULL) {
        sixel_frame_unref(assessment_source_frame);
    }
    if (encoder->assessment_observer != NULL) {
        sixel_assessment_unref(encoder->assessment_observer);
        encoder->assessment_observer = NULL;
    }
    if (assessment_allocator != NULL) {
        sixel_allocator_unref(assessment_allocator);
    }

    sixel_encoder_unref(encoder);

    return status;
}


/* encode specified pixel data to SIXEL format
 * output to encoder->outfd */
SIXELAPI SIXELSTATUS
sixel_encoder_encode_bytes(
    sixel_encoder_t     /* in */    *encoder,
    unsigned char       /* in */    *bytes,
    int                 /* in */    width,
    int                 /* in */    height,
    int                 /* in */    pixelformat,
    unsigned char       /* in */    *palette,
    int                 /* in */    ncolors)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame = NULL;

    if (encoder == NULL || bytes == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = sixel_frame_new(&frame, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_frame_init(frame, bytes, width, height,
                              pixelformat, palette, ncolors);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_encoder_encode_frame(encoder, frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = SIXEL_OK;

end:
    /* we need to free the frame before exiting, but we can't use the
       sixel_frame_destroy function, because that will also attempt to
       free the pixels and palette, which we don't own */
    if (frame != NULL && encoder->allocator != NULL) {
        sixel_allocator_free(encoder->allocator, frame);
        sixel_allocator_unref(encoder->allocator);
    }
    return status;
}


/*
 * Toggle source-frame capture for assessment consumers.
 */
SIXELAPI SIXELSTATUS
sixel_encoder_enable_source_capture(
    sixel_encoder_t *encoder,
    int enable)
{
    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_enable_source_capture: encoder is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->capture_source = enable ? 1 : 0;
    if (!encoder->capture_source && encoder->capture_source_frame != NULL) {
        sixel_frame_unref(encoder->capture_source_frame);
        encoder->capture_source_frame = NULL;
    }

    return SIXEL_OK;
}


/*
 * Enable or disable the quantized-frame capture facility.
 *
 *     capture on --> encoder keeps the latest palette-quantized frame.
 *     capture off --> encoder forgets previously stored frames.
 */
SIXELAPI SIXELSTATUS
sixel_encoder_enable_quantized_capture(
    sixel_encoder_t *encoder,
    int enable)
{
    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_enable_quantized_capture: encoder is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    encoder->capture_quantized = enable ? 1 : 0;
    if (!encoder->capture_quantized) {
        encoder->capture_valid = 0;
    }

    return SIXEL_OK;
}


/*
 * Materialize the captured quantized frame as a heap-allocated
 * sixel_frame_t instance for assessment consumers.
 */
SIXELAPI SIXELSTATUS
sixel_encoder_copy_quantized_frame(
    sixel_encoder_t   *encoder,
    sixel_allocator_t *allocator,
    sixel_frame_t     **ppframe)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_frame_t *frame;
    unsigned char *pixels;
    unsigned char *palette;
    size_t palette_bytes;

    if (encoder == NULL || allocator == NULL || ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_copy_quantized_frame: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (!encoder->capture_quantized || !encoder->capture_valid) {
        sixel_helper_set_additional_message(
            "sixel_encoder_copy_quantized_frame: no frame captured.");
        return SIXEL_RUNTIME_ERROR;
    }

    *ppframe = NULL;
    frame = NULL;
    pixels = NULL;
    palette = NULL;

    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    if (encoder->capture_pixel_bytes > 0) {
        pixels = (unsigned char *)sixel_allocator_malloc(
            allocator, encoder->capture_pixel_bytes);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_copy_quantized_frame: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        memcpy(pixels,
               encoder->capture_pixels,
               encoder->capture_pixel_bytes);
    }

    palette_bytes = encoder->capture_palette_size;
    if (palette_bytes > 0) {
        palette = (unsigned char *)sixel_allocator_malloc(allocator,
                                                          palette_bytes);
        if (palette == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_copy_quantized_frame: "
                "sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            goto cleanup;
        }
        memcpy(palette,
               encoder->capture_palette,
               palette_bytes);
    }

    status = sixel_frame_init(frame,
                              pixels,
                              encoder->capture_width,
                              encoder->capture_height,
                              encoder->capture_pixelformat,
                              palette,
                              encoder->capture_ncolors);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    pixels = NULL;
    palette = NULL;
    frame->colorspace = encoder->capture_colorspace;
    *ppframe = frame;
    return SIXEL_OK;

cleanup:
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (pixels != NULL) {
        sixel_allocator_free(allocator, pixels);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }
    return status;
}


/*
 * Emit the captured palette in the requested format.
 *
 *   palette_output == NULL  -> skip
 *   palette_output != NULL  -> materialize captured palette
 */
static SIXELSTATUS
sixel_encoder_emit_palette_output(sixel_encoder_t *encoder)
{
    SIXELSTATUS status;
    sixel_frame_t *frame;
    unsigned char const *palette;
    int exported_colors;
    FILE *stream;
    int close_stream;
    char const *path;
    sixel_palette_format_t format_hint;
    sixel_palette_format_t format_ext;
    sixel_palette_format_t format_final;
    char const *mode;

    status = SIXEL_OK;
    frame = NULL;
    palette = NULL;
    exported_colors = 0;
    stream = NULL;
    close_stream = 0;
    path = NULL;
    format_hint = SIXEL_PALETTE_FORMAT_NONE;
    format_ext = SIXEL_PALETTE_FORMAT_NONE;
    format_final = SIXEL_PALETTE_FORMAT_NONE;
    mode = "wb";

    if (encoder == NULL || encoder->palette_output == NULL) {
        return SIXEL_OK;
    }

    status = sixel_encoder_copy_quantized_frame(encoder,
                                                encoder->allocator,
                                                &frame);
    if (SIXEL_FAILED(status)) {
        return status;
    }

    palette = (unsigned char const *)sixel_frame_get_palette(frame);
    exported_colors = sixel_frame_get_ncolors(frame);
    if (palette == NULL || exported_colors <= 0) {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_palette_output: palette unavailable.");
        status = SIXEL_BAD_INPUT;
        goto cleanup;
    }
    if (exported_colors > 256) {
        exported_colors = 256;
    }

    path = sixel_palette_strip_prefix(encoder->palette_output, &format_hint);
    if (path == NULL || *path == '\0') {
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_palette_output: invalid path.");
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    format_ext = sixel_palette_format_from_extension(path);
    format_final = format_hint;
    if (format_final == SIXEL_PALETTE_FORMAT_NONE) {
        if (format_ext == SIXEL_PALETTE_FORMAT_NONE) {
            if (strcmp(path, "-") == 0) {
                sixel_helper_set_additional_message(
                    "sixel_encoder_emit_palette_output: "
                    "format required for '-'.");
                status = SIXEL_BAD_ARGUMENT;
                goto cleanup;
            }
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: "
                "unknown palette file extension.");
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }
        format_final = format_ext;
    }
    if (format_final == SIXEL_PALETTE_FORMAT_PAL_AUTO) {
        format_final = SIXEL_PALETTE_FORMAT_PAL_JASC;
    }

    if (strcmp(path, "-") == 0) {
        stream = stdout;
    } else {
        if (format_final == SIXEL_PALETTE_FORMAT_PAL_JASC ||
                format_final == SIXEL_PALETTE_FORMAT_GPL) {
            mode = "w";
        } else {
            mode = "wb";
        }
        stream = fopen(path, mode);
        if (stream == NULL) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to open file.");
            status = SIXEL_LIBC_ERROR;
            goto cleanup;
        }
        close_stream = 1;
    }

    switch (format_final) {
    case SIXEL_PALETTE_FORMAT_ACT:
        status = sixel_palette_write_act(stream, palette, exported_colors);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to write ACT.");
        }
        break;
    case SIXEL_PALETTE_FORMAT_PAL_JASC:
        status = sixel_palette_write_pal_jasc(stream,
                                              palette,
                                              exported_colors);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to write JASC.");
        }
        break;
    case SIXEL_PALETTE_FORMAT_PAL_RIFF:
        status = sixel_palette_write_pal_riff(stream,
                                              palette,
                                              exported_colors);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to write RIFF.");
        }
        break;
    case SIXEL_PALETTE_FORMAT_GPL:
        status = sixel_palette_write_gpl(stream,
                                         palette,
                                         exported_colors);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: failed to write GPL.");
        }
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_encoder_emit_palette_output: unsupported format.");
        status = SIXEL_BAD_ARGUMENT;
        break;
    }
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (close_stream) {
        if (fclose(stream) != 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: fclose() failed.");
            status = SIXEL_LIBC_ERROR;
            stream = NULL;
            goto cleanup;
        }
        stream = NULL;
    } else {
        if (fflush(stream) != 0) {
            sixel_helper_set_additional_message(
                "sixel_encoder_emit_palette_output: fflush() failed.");
            status = SIXEL_LIBC_ERROR;
            goto cleanup;
        }
    }

cleanup:
    if (close_stream && stream != NULL) {
        (void) fclose(stream);
    }
    if (frame != NULL) {
        sixel_frame_unref(frame);
    }

    return status;
}


/*
 * Share the captured source frame with assessment consumers.
 */
SIXELAPI SIXELSTATUS
sixel_encoder_copy_source_frame(
    sixel_encoder_t *encoder,
    sixel_frame_t  **ppframe)
{
    if (encoder == NULL || ppframe == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_copy_source_frame: invalid argument.");
        return SIXEL_BAD_ARGUMENT;
    }

    if (!encoder->capture_source || encoder->capture_source_frame == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_copy_source_frame: no frame captured.");
        return SIXEL_RUNTIME_ERROR;
    }

    sixel_frame_ref(encoder->capture_source_frame);
    *ppframe = encoder->capture_source_frame;

    return SIXEL_OK;
}


#if HAVE_TESTS
static int
test1(void)
{
    int nret = EXIT_FAILURE;
    sixel_encoder_t *encoder = NULL;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (encoder == NULL) {
        goto error;
    }
    sixel_encoder_ref(encoder);
    sixel_encoder_unref(encoder);
    nret = EXIT_SUCCESS;

error:
    sixel_encoder_unref(encoder);
    return nret;
}


static int
test2(void)
{
    int nret = EXIT_FAILURE;
    SIXELSTATUS status;
    sixel_encoder_t *encoder = NULL;
    sixel_frame_t *frame = NULL;
    unsigned char *buffer;
    int height = 0;
    int is_animation = 0;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (encoder == NULL) {
        goto error;
    }

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    frame = sixel_frame_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
#  pragma GCC diagnostic pop
#endif
    if (encoder == NULL) {
        goto error;
    }

    buffer = (unsigned char *)sixel_allocator_malloc(encoder->allocator, 3);
    if (buffer == NULL) {
        goto error;
    }
    status = sixel_frame_init(frame, buffer, 1, 1,
                              SIXEL_PIXELFORMAT_RGB888,
                              NULL, 0);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    if (sixel_frame_get_loop_no(frame) != 0 || sixel_frame_get_frame_no(frame) != 0) {
        is_animation = 1;
    }

    height = sixel_frame_get_height(frame);

    status = sixel_tty_scroll(sixel_write_callback,
                              &encoder->outfd,
                              encoder->outfd,
                              height,
                              is_animation);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    return nret;
}


static int
test3(void)
{
    int nret = EXIT_FAILURE;
    int result;

    result = sixel_tty_wait_stdin(1000);
    if (result != 0) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}


static int
test4(void)
{
    int nret = EXIT_FAILURE;
    sixel_encoder_t *encoder = NULL;
    SIXELSTATUS status;

#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
    encoder = sixel_encoder_create();
#if HAVE_DIAGNOSTIC_DEPRECATED_DECLARATIONS
# pragma GCC diagnostic pop
#endif
    if (encoder == NULL) {
        goto error;
    }

    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_LOOPMODE,
                                  "force");
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_encoder_setopt(encoder,
                                  SIXEL_OPTFLAG_PIPE_MODE,
                                  "force");
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    nret = EXIT_SUCCESS;

error:
    sixel_encoder_unref(encoder);
    return nret;
}


static int
test5(void)
{
    int nret = EXIT_FAILURE;
    sixel_encoder_t *encoder = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    status = sixel_encoder_new(&encoder, allocator);
    if (SIXEL_FAILED(status)) {
        goto error;
    }

    sixel_encoder_ref(encoder);
    sixel_encoder_unref(encoder);
    nret = EXIT_SUCCESS;

error:
    sixel_encoder_unref(encoder);
    return nret;
}


SIXELAPI int
sixel_encoder_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
        test3,
        test4,
        test5
    };

    for (i = 0; i < sizeof(testcases) / sizeof(testcase); ++i) {
        nret = testcases[i]();
        if (nret != EXIT_SUCCESS) {
            goto error;
        }
    }

    nret = EXIT_SUCCESS;

error:
    return nret;
}
#endif  /* HAVE_TESTS */


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 : */
/* EOF */
