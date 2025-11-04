/*
 * SPDX-License-Identifier: MIT
 */

#include "config.h"

#include <string.h>

#include "clipboard.h"
#include "allocator.h"

static int
clipboard_parse_format(char const *spec,
                       sixel_clipboard_spec_t *out)
{
    size_t prefix_len;
    char const *marker;

    prefix_len = 0u;
    marker = NULL;

    if (out == NULL) {
        return 0;
    }

    out->is_clipboard = 0;
    out->format[0] = '\0';

    if (spec == NULL) {
        return 0;
    }

    marker = strstr(spec, "clipboard:");
    if (marker == NULL) {
        return 0;
    }

    if (marker[10] != '\0') {
        return 0;
    }

    out->is_clipboard = 1;

    prefix_len = (size_t)(marker - spec);
    if (prefix_len == 0u) {
        out->format[0] = '\0';
        return 1;
    }

    if (spec[prefix_len - 1u] != ':') {
        out->is_clipboard = 0;
        return 0;
    }

    --prefix_len;
    if (prefix_len >= sizeof(out->format)) {
        prefix_len = sizeof(out->format) - 1u;
    }

    memcpy(out->format, spec, prefix_len);
    out->format[prefix_len] = '\0';

    return 1;
}

int
sixel_clipboard_parse_spec(char const *spec,
                           sixel_clipboard_spec_t *out)
{
    return clipboard_parse_format(spec, out);
}

#if defined(HAVE_CLIPBOARD_MACOS)

SIXELSTATUS sixel_clipboard_read_macos(char const *format,
                                       unsigned char **data,
                                       size_t *size,
                                       sixel_allocator_t *allocator);
SIXELSTATUS sixel_clipboard_write_macos(char const *format,
                                        unsigned char const *data,
                                        size_t size);
int sixel_clipboard_is_available_macos(void);

#endif

SIXELSTATUS
sixel_clipboard_read(char const *format,
                     unsigned char **data,
                     size_t *size,
                     sixel_allocator_t *allocator)
{
#if defined(HAVE_CLIPBOARD_MACOS)
    return sixel_clipboard_read_macos(format, data, size, allocator);
#else
    (void) format;
    (void) data;
    (void) size;
    (void) allocator;
    sixel_helper_set_additional_message(
        "clipboard backend not available.");
    return SIXEL_NOT_IMPLEMENTED;
#endif
}

SIXELSTATUS
sixel_clipboard_write(char const *format,
                      unsigned char const *data,
                      size_t size)
{
#if defined(HAVE_CLIPBOARD_MACOS)
    return sixel_clipboard_write_macos(format, data, size);
#else
    (void) format;
    (void) data;
    (void) size;
    sixel_helper_set_additional_message(
        "clipboard backend not available.");
    return SIXEL_NOT_IMPLEMENTED;
#endif
}

int
sixel_clipboard_is_available(void)
{
#if defined(HAVE_CLIPBOARD_MACOS)
    return sixel_clipboard_is_available_macos();
#else
    return 0;
#endif
}

