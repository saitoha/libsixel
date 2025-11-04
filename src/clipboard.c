/*
 * SPDX-License-Identifier: MIT
 */

#if defined(HAVE_CLIPBOARD_LINUX) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#if defined(HAVE_CLIPBOARD_LINUX)
#include <errno.h>
#include <unistd.h>

FILE *popen(char const *command, char const *mode);
int pclose(FILE *stream);
#endif

#if defined(HAVE_CLIPBOARD_WINDOWS)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wchar.h>
#include <limits.h>
#endif

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

#elif defined(HAVE_CLIPBOARD_WINDOWS)

/*
 * Cache the registered PNG clipboard format identifier so we can reuse the
 * Windows allocation between subsequent read and write operations.
 */
static UINT
clipboard_windows_png_format(void)
{
    static UINT png_format = 0u;

    if (png_format == 0u) {
        png_format = RegisterClipboardFormatW(L"PNG");
    }

    return png_format;
}

/*
 * Duplicate a clipboard HGLOBAL payload into heap memory owned by the
 * caller.
 *
 *   1. Validate the output pointers so the caller receives predictable
 *      results.
 *   2. Lock the clipboard handle to access the raw bytes.
 *   3. Allocate a destination buffer and copy the payload.
 */
static SIXELSTATUS
clipboard_windows_duplicate_hglobal(HGLOBAL handle,
                                    unsigned char **buffer_out,
                                    size_t *length_out)
{
    SIZE_T handle_size;
    unsigned char *buffer;
    void *view;

    handle_size = 0u;
    buffer = NULL;
    view = NULL;

    if (buffer_out == NULL || length_out == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: destination pointers are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *buffer_out = NULL;
    *length_out = 0u;

    if (handle == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: clipboard handle is null.");
        return SIXEL_RUNTIME_ERROR;
    }

    handle_size = GlobalSize(handle);
    if (handle_size == 0u) {
        *buffer_out = NULL;
        *length_out = 0u;
        return SIXEL_OK;
    }

    view = GlobalLock(handle);
    if (view == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: GlobalLock() failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    buffer = (unsigned char *)malloc((size_t)handle_size);
    if (buffer == NULL) {
        GlobalUnlock(handle);
        sixel_helper_set_additional_message(
            "clipboard: malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memcpy(buffer, view, (size_t)handle_size);
    GlobalUnlock(handle);

    *buffer_out = buffer;
    *length_out = (size_t)handle_size;
    return SIXEL_OK;
}

/*
 * Probe the clipboard for the registered PNG payload and copy it into the
 * library allocator.
 */
static SIXELSTATUS
clipboard_windows_read_png(unsigned char **data,
                           size_t *size)
{
    SIXELSTATUS status;
    UINT png_format;
    HANDLE raw_handle;

    status = SIXEL_FALSE;
    png_format = 0u;
    raw_handle = NULL;

    png_format = clipboard_windows_png_format();
    if (png_format == 0u) {
        sixel_helper_set_additional_message(
            "clipboard: RegisterClipboardFormatW(\"PNG\") failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    if (!IsClipboardFormatAvailable(png_format)) {
        return SIXEL_BAD_CLIPBOARD;
    }

    raw_handle = GetClipboardData(png_format);
    if (raw_handle == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: GetClipboardData() for PNG failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    status = clipboard_windows_duplicate_hglobal((HGLOBAL)raw_handle,
                                                 data,
                                                 size);
    return status;
}

/*
 * Convert UTF-16 clipboard text into an UTF-8 buffer managed by libsixel.
 */
static SIXELSTATUS
clipboard_windows_read_text(unsigned char **data,
                            size_t *size)
{
    HANDLE raw_handle;
    wchar_t const *wide_text;
    size_t wide_length;
    int required_bytes;
    unsigned char *buffer;
    int convert_result;

    raw_handle = NULL;
    wide_text = NULL;
    wide_length = 0u;
    required_bytes = 0;
    buffer = NULL;
    convert_result = 0;

    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return SIXEL_BAD_CLIPBOARD;
    }

    raw_handle = GetClipboardData(CF_UNICODETEXT);
    if (raw_handle == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: GetClipboardData() for text failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    wide_text = (wchar_t const *)GlobalLock(raw_handle);
    if (wide_text == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: GlobalLock() for text failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    wide_length = wcslen(wide_text);
    if (wide_length == 0u) {
        GlobalUnlock(raw_handle);
        *data = NULL;
        *size = 0u;
        return SIXEL_OK;
    }

    if (wide_length > (size_t)INT_MAX) {
        GlobalUnlock(raw_handle);
        sixel_helper_set_additional_message(
            "clipboard: text payload is too large to convert.");
        return SIXEL_RUNTIME_ERROR;
    }

    required_bytes = WideCharToMultiByte(CP_UTF8,
                                         WC_ERR_INVALID_CHARS,
                                         wide_text,
                                         (int)wide_length,
                                         NULL,
                                         0,
                                         NULL,
                                         NULL);
    if (required_bytes == 0) {
        GlobalUnlock(raw_handle);
        sixel_helper_set_additional_message(
            "clipboard: WideCharToMultiByte() sizing failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    buffer = (unsigned char *)malloc((size_t)required_bytes);
    if (buffer == NULL) {
        GlobalUnlock(raw_handle);
        sixel_helper_set_additional_message(
            "clipboard: malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    convert_result = WideCharToMultiByte(CP_UTF8,
                                         WC_ERR_INVALID_CHARS,
                                         wide_text,
                                         (int)wide_length,
                                         (LPSTR)buffer,
                                         required_bytes,
                                         NULL,
                                         NULL);
    GlobalUnlock(raw_handle);
    if (convert_result == 0) {
        free(buffer);
        sixel_helper_set_additional_message(
            "clipboard: WideCharToMultiByte() conversion failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    *data = buffer;
    *size = (size_t)required_bytes;
    return SIXEL_OK;
}

/*
 * Inspect the optional "format:" prefix and decide whether the caller wants
 * textual SIXEL data or a binary PNG blob.
 */
static UINT
clipboard_windows_format_for_write(char const *format,
                                   int *treat_as_text)
{
    char lowered[32];
    size_t length;
    size_t index;

    lowered[0] = '\0';
    length = 0u;
    index = 0u;

    *treat_as_text = 0;

    if (format != NULL) {
        length = strlen(format);
        if (length >= sizeof(lowered)) {
            length = sizeof(lowered) - 1u;
        }
        for (index = 0u; index < length; ++index) {
            lowered[index] = (char)tolower((unsigned char)format[index]);
        }
        lowered[length] = '\0';
    }

    if (lowered[0] == '\0') {
        *treat_as_text = 1;
        return CF_UNICODETEXT;
    }
    if (strcmp(lowered, "png") == 0) {
        return clipboard_windows_png_format();
    }

    *treat_as_text = 1;
    return CF_UNICODETEXT;
}

/*
 * Emit UTF-8 data to the clipboard using the CF_UNICODETEXT format and rely
 * on Windows to manage the global memory handle after SetClipboardData().
 */
static SIXELSTATUS
clipboard_windows_write_text(unsigned char const *data,
                             size_t size)
{
    HGLOBAL handle;
    wchar_t *wide_buffer;
    size_t wide_length;
    int required_length;
    int convert_result;
    size_t allocation_size;

    handle = NULL;
    wide_buffer = NULL;
    wide_length = 0u;
    required_length = 0;
    convert_result = 0;
    allocation_size = 0u;

    if (size > (size_t)INT_MAX) {
        sixel_helper_set_additional_message(
            "clipboard: text payload exceeds Windows limits.");
        return SIXEL_RUNTIME_ERROR;
    }

    if (size == 0u || data == NULL) {
        required_length = 0;
    } else {
        required_length = MultiByteToWideChar(CP_UTF8,
                                              MB_ERR_INVALID_CHARS,
                                              (LPCCH)data,
                                              (int)size,
                                              NULL,
                                              0);
        if (required_length == 0) {
            sixel_helper_set_additional_message(
                "clipboard: MultiByteToWideChar() sizing failed.");
            return SIXEL_RUNTIME_ERROR;
        }
    }

    wide_length = (size_t)required_length;
    allocation_size = (wide_length + 1u) * sizeof(wchar_t);
    handle = GlobalAlloc(GMEM_MOVEABLE, allocation_size);
    if (handle == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: GlobalAlloc() failed for text payload.");
        return SIXEL_BAD_ALLOCATION;
    }

    wide_buffer = (wchar_t *)GlobalLock(handle);
    if (wide_buffer == NULL) {
        GlobalFree(handle);
        sixel_helper_set_additional_message(
            "clipboard: GlobalLock() failed for text payload.");
        return SIXEL_RUNTIME_ERROR;
    }

    if (wide_length > 0u) {
        convert_result = MultiByteToWideChar(CP_UTF8,
                                             MB_ERR_INVALID_CHARS,
                                             (LPCCH)data,
                                             (int)size,
                                             wide_buffer,
                                             required_length);
        if (convert_result == 0) {
            GlobalUnlock(handle);
            GlobalFree(handle);
            sixel_helper_set_additional_message(
                "clipboard: MultiByteToWideChar() conversion failed.");
            return SIXEL_RUNTIME_ERROR;
        }
    }

    wide_buffer[wide_length] = L'\0';
    GlobalUnlock(handle);

    if (SetClipboardData(CF_UNICODETEXT, handle) == NULL) {
        GlobalFree(handle);
        sixel_helper_set_additional_message(
            "clipboard: SetClipboardData() for text failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    return SIXEL_OK;
}

/*
 * Transfer a PNG blob to the clipboard by allocating a movable global handle
 * and delegating ownership to the system clipboard API.
 */
static SIXELSTATUS
clipboard_windows_write_png(unsigned char const *data,
                            size_t size)
{
    HGLOBAL handle;
    unsigned char *view;
    UINT png_format;
    size_t allocation_size;

    handle = NULL;
    view = NULL;
    png_format = 0u;
    allocation_size = 0u;

    png_format = clipboard_windows_png_format();
    if (png_format == 0u) {
        sixel_helper_set_additional_message(
            "clipboard: RegisterClipboardFormatW(\"PNG\") failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    allocation_size = size == 0u ? 1u : size;
    handle = GlobalAlloc(GMEM_MOVEABLE, allocation_size);
    if (handle == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: GlobalAlloc() failed for PNG payload.");
        return SIXEL_BAD_ALLOCATION;
    }

    view = (unsigned char *)GlobalLock(handle);
    if (view == NULL) {
        GlobalFree(handle);
        sixel_helper_set_additional_message(
            "clipboard: GlobalLock() failed for PNG payload.");
        return SIXEL_RUNTIME_ERROR;
    }

    if (size > 0u && data != NULL) {
        memcpy(view, data, size);
    }
    GlobalUnlock(handle);

    if (SetClipboardData(png_format, handle) == NULL) {
        GlobalFree(handle);
        sixel_helper_set_additional_message(
            "clipboard: SetClipboardData() for PNG failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    return SIXEL_OK;
}

/*
 * Windows clipboard entry point used by the higher level encoder and decoder
 * routines.  The method prefers PNG blobs and falls back to textual SIXEL
 * sequences when PNG is unavailable.
 */
SIXELSTATUS
sixel_clipboard_read_windows(char const *format,
                             unsigned char **data,
                             size_t *size,
                             sixel_allocator_t *allocator)
{
    SIXELSTATUS status;
    SIXELSTATUS png_status;
    SIXELSTATUS text_status;
    unsigned char *buffer;
    size_t length;

    status = SIXEL_FALSE;
    png_status = SIXEL_FALSE;
    text_status = SIXEL_FALSE;
    buffer = NULL;
    length = 0u;

    (void) format;
    (void) allocator;

    if (data == NULL || size == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: destination pointers are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *data = NULL;
    *size = 0u;

    if (!OpenClipboard(NULL)) {
        sixel_helper_set_additional_message(
            "clipboard: OpenClipboard() failed.");
        return SIXEL_BAD_CLIPBOARD;
    }

    png_status = clipboard_windows_read_png(&buffer, &length);
    if (png_status == SIXEL_OK) {
        status = SIXEL_OK;
        goto cleanup;
    }
    if (png_status != SIXEL_BAD_CLIPBOARD) {
        status = png_status;
        goto cleanup;
    }

    text_status = clipboard_windows_read_text(&buffer, &length);
    if (text_status == SIXEL_OK) {
        status = SIXEL_OK;
        goto cleanup;
    }
    if (text_status == SIXEL_BAD_CLIPBOARD) {
        sixel_helper_set_additional_message(
            "clipboard: PNG and text payloads are unavailable.");
        status = SIXEL_BAD_CLIPBOARD;
    } else {
        status = text_status;
    }

cleanup:
    CloseClipboard();
    if (SIXEL_SUCCEEDED(status)) {
        *data = buffer;
        *size = length;
    } else {
        if (buffer != NULL) {
            free(buffer);
        }
    }

    return status;
}

/*
 * Windows clipboard write backend that supports either textual SIXEL data or
 * PNG binaries depending on the caller's requested format.
 */
SIXELSTATUS
sixel_clipboard_write_windows(char const *format,
                              unsigned char const *data,
                              size_t size)
{
    SIXELSTATUS status;
    int treat_as_text;
    UINT target_format;
    SIXELSTATUS write_status;

    status = SIXEL_FALSE;
    treat_as_text = 0;
    target_format = 0u;
    write_status = SIXEL_FALSE;

    if (data == NULL && size > 0u) {
        sixel_helper_set_additional_message(
            "clipboard: source buffer is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    target_format = clipboard_windows_format_for_write(format,
                                                       &treat_as_text);
    if (treat_as_text == 0 && target_format == 0u) {
        sixel_helper_set_additional_message(
            "clipboard: PNG format registration failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    if (!OpenClipboard(NULL)) {
        sixel_helper_set_additional_message(
            "clipboard: OpenClipboard() failed.");
        return SIXEL_BAD_CLIPBOARD;
    }

    if (!EmptyClipboard()) {
        CloseClipboard();
        sixel_helper_set_additional_message(
            "clipboard: EmptyClipboard() failed.");
        return SIXEL_RUNTIME_ERROR;
    }

    if (treat_as_text) {
        write_status = clipboard_windows_write_text(data, size);
    } else {
        write_status = clipboard_windows_write_png(data, size);
    }

    if (write_status != SIXEL_OK) {
        CloseClipboard();
        return write_status;
    }

    status = SIXEL_OK;
    CloseClipboard();
    return status;
}

/*
 * Lightweight availability probe used by unit tests to decide whether they
 * should attempt clipboard integration tests.
 */
int
sixel_clipboard_is_available_windows(void)
{
    int available;

    available = 0;

    if (OpenClipboard(NULL)) {
        available = 1;
        CloseClipboard();
    }

    return available;
}

#elif defined(HAVE_CLIPBOARD_LINUX)

typedef struct clipboard_linux_caps {
    int probed;
    int have_wl_copy;
    int have_wl_paste;
    int have_xclip;
} clipboard_linux_caps_t;

static char const clipboard_linux_cmd_wl_copy_text[] =
    "wl-copy --type text/plain;charset=utf-8";
static char const clipboard_linux_cmd_wl_copy_png[] =
    "wl-copy --type image/png";
static char const clipboard_linux_cmd_wl_paste_text[] =
    "wl-paste --no-newline";
static char const clipboard_linux_cmd_wl_paste_png[] =
    "wl-paste --type image/png --no-newline";
static char const clipboard_linux_cmd_xclip_write_text[] =
    "xclip -selection clipboard -in";
static char const clipboard_linux_cmd_xclip_write_png[] =
    "xclip -selection clipboard -t image/png -in";
static char const clipboard_linux_cmd_xclip_read_text[] =
    "xclip -selection clipboard -out";
static char const clipboard_linux_cmd_xclip_read_png[] =
    "xclip -selection clipboard -t image/png -out";

#define CLIPBOARD_LINUX_PATH_LIMIT 4096u

/*
 * Release a temporary buffer using the allocator selected by the caller so
 * the higher layers can retry alternative clipboard helpers without leaking
 * memory.
 */
static void
clipboard_linux_free_buffer(unsigned char *buffer,
                            sixel_allocator_t *allocator)
{
    if (buffer == NULL) {
        return;
    }
    if (allocator != NULL) {
        sixel_allocator_free(allocator, buffer);
    } else {
        free(buffer);
    }
}

/*
 * Locate a helper binary by walking the PATH environment variable.  The scan
 * follows this sequence:
 *
 *   1. Iterate over each PATH component.
 *   2. Assemble a candidate path that joins the directory and program name.
 *   3. Probe the candidate with access(2) and stop once an executable is
 *      discovered.
 */
static int
clipboard_linux_find_program(char const *name)
{
    char const *path_env;
    char const *cursor;
    char const *next;
    size_t name_length;
    size_t segment_length;
    size_t directory_length;
    size_t required_length;
    char directory[CLIPBOARD_LINUX_PATH_LIMIT];
    char candidate[CLIPBOARD_LINUX_PATH_LIMIT];
    int available;
    int written;

    path_env = NULL;
    cursor = NULL;
    next = NULL;
    name_length = 0u;
    segment_length = 0u;
    directory_length = 0u;
    required_length = 0u;
    available = 0;
    written = 0;

    if (name == NULL) {
        return 0;
    }

    name_length = strlen(name);
    if (name_length == 0u) {
        return 0;
    }

    path_env = getenv("PATH");
    if (path_env == NULL) {
        return 0;
    }

    cursor = path_env;
    while (available == 0 && cursor != NULL) {
        next = strchr(cursor, ':');
        if (next == NULL) {
            segment_length = strlen(cursor);
        } else {
            segment_length = (size_t)(next - cursor);
        }

        if (segment_length >= sizeof(directory)) {
            segment_length = sizeof(directory) - 1u;
        }

        if (segment_length > 0u) {
            memcpy(directory, cursor, segment_length);
            directory[segment_length] = '\0';
        } else {
            directory[0] = '.';
            directory[1] = '\0';
            segment_length = 1u;
        }

        directory_length = strlen(directory);
        required_length = directory_length + 1u + name_length + 1u;
        if (required_length <= sizeof(candidate)) {
            if (directory_length > 0u &&
                    directory[directory_length - 1u] == '/') {
                written = snprintf(candidate, sizeof(candidate),
                                    "%s%s", directory, name);
            } else {
                written = snprintf(candidate, sizeof(candidate),
                                    "%s/%s", directory, name);
            }
            if (written > 0 && (size_t)written < sizeof(candidate)) {
                if (access(candidate, X_OK) == 0) {
                    available = 1;
                }
            }
        }

        if (next == NULL) {
            break;
        }
        cursor = next + 1;
    }

    return available;
}

/*
 * Cache the discovery of helper binaries so repeated read and write requests
 * do not touch the filesystem more than once per process.
 */
static void
clipboard_linux_detect_capabilities(clipboard_linux_caps_t *caps)
{
    static clipboard_linux_caps_t cached;
    static int cached_valid = 0;
    clipboard_linux_caps_t result;

    result.probed = 0;
    result.have_wl_copy = 0;
    result.have_wl_paste = 0;
    result.have_xclip = 0;

    if (caps == NULL) {
        return;
    }

    if (cached_valid) {
        *caps = cached;
        return;
    }

    result.probed = 1;
    result.have_wl_copy = clipboard_linux_find_program("wl-copy");
    result.have_wl_paste = clipboard_linux_find_program("wl-paste");
    result.have_xclip = clipboard_linux_find_program("xclip");

    cached = result;
    cached_valid = 1;

    *caps = cached;
}

/*
 * Decide whether the caller explicitly requested a PNG payload.  Any other
 * format string defaults to textual SIXEL data.
 */
static int
clipboard_linux_request_png(char const *format)
{
    char lowered[32];
    size_t length;
    size_t index;
    int is_png;

    lowered[0] = '\0';
    length = 0u;
    index = 0u;
    is_png = 0;

    if (format == NULL) {
        return 0;
    }

    length = strlen(format);
    if (length >= sizeof(lowered)) {
        length = sizeof(lowered) - 1u;
    }
    for (index = 0u; index < length; ++index) {
        lowered[index] = (char)tolower((unsigned char)format[index]);
    }
    lowered[length] = '\0';

    if (lowered[0] == '\0') {
        return 0;
    }
    if (strcmp(lowered, "png") == 0) {
        is_png = 1;
    }
    return is_png;
}

/*
 * Spawn a clipboard helper, collect its stdout into a dynamically sized
 * buffer, and surface the data back to the caller.  The steps are:
 *
 *   1. Launch the helper with popen().
 *   2. Read until EOF while growing the buffer as needed.
 *   3. Close the pipe and honour the helper's exit status.
 */
static SIXELSTATUS
clipboard_linux_read_command(char const *command,
                             unsigned char **data,
                             size_t *size,
                             sixel_allocator_t *allocator)
{
    FILE *pipe_handle;
    unsigned char *buffer;
    unsigned char *resized;
    size_t capacity;
    size_t length;
    size_t chunk;
    size_t new_capacity;
    int close_status;

    pipe_handle = NULL;
    buffer = NULL;
    resized = NULL;
    capacity = 0u;
    length = 0u;
    chunk = 0u;
    new_capacity = 0u;
    close_status = 0;

    if (command == NULL || data == NULL || size == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: invalid command arguments.");
        return SIXEL_BAD_ARGUMENT;
    }

    *data = NULL;
    *size = 0u;

    pipe_handle = popen(command, "r");
    if (pipe_handle == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: popen() failed for reader.");
        return SIXEL_RUNTIME_ERROR;
    }

    capacity = 4096u;
    if (allocator != NULL) {
        buffer = (unsigned char *)sixel_allocator_malloc(allocator,
                                                         capacity);
    } else {
        buffer = (unsigned char *)malloc(capacity);
    }
    if (buffer == NULL) {
        pclose(pipe_handle);
        sixel_helper_set_additional_message(
            "clipboard: unable to allocate read buffer.");
        return SIXEL_BAD_ALLOCATION;
    }

    length = 0u;
    while (1) {
        chunk = fread(buffer + length, 1u, capacity - length, pipe_handle);
        length += chunk;
        if (chunk == 0u) {
            if (ferror(pipe_handle)) {
                clipboard_linux_free_buffer(buffer, allocator);
                pclose(pipe_handle);
                sixel_helper_set_additional_message(
                    "clipboard: fread() failed.");
                return SIXEL_RUNTIME_ERROR;
            }
            break;
        }
        if (capacity - length < 1024u) {
            new_capacity = capacity * 2u;
            if (new_capacity <= capacity) {
                clipboard_linux_free_buffer(buffer, allocator);
                pclose(pipe_handle);
                sixel_helper_set_additional_message(
                    "clipboard: buffer growth overflow.");
                return SIXEL_RUNTIME_ERROR;
            }
            if (allocator != NULL) {
                resized = (unsigned char *)sixel_allocator_realloc(
                    allocator, buffer, new_capacity);
            } else {
                resized = (unsigned char *)realloc(buffer, new_capacity);
            }
            if (resized == NULL) {
                clipboard_linux_free_buffer(buffer, allocator);
                pclose(pipe_handle);
                sixel_helper_set_additional_message(
                    "clipboard: realloc() failed during read.");
                return SIXEL_BAD_ALLOCATION;
            }
            buffer = resized;
            capacity = new_capacity;
        }
    }

    close_status = pclose(pipe_handle);
    if (close_status != 0) {
        clipboard_linux_free_buffer(buffer, allocator);
        sixel_helper_set_additional_message(
            "clipboard: helper exited with failure.");
        return SIXEL_BAD_CLIPBOARD;
    }

    *data = buffer;
    *size = length;
    return SIXEL_OK;
}

/*
 * Forward a payload to a helper command via stdin and observe the exit status
 * so we can distinguish clipboard errors from programming mistakes.
 */
static SIXELSTATUS
clipboard_linux_write_command(char const *command,
                              unsigned char const *data,
                              size_t size)
{
    FILE *pipe_handle;
    size_t offset;
    size_t written;
    int close_status;

    pipe_handle = NULL;
    offset = 0u;
    written = 0u;
    close_status = 0;

    if (command == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: invalid command arguments.");
        return SIXEL_BAD_ARGUMENT;
    }

    pipe_handle = popen(command, "w");
    if (pipe_handle == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: popen() failed for writer.");
        return SIXEL_RUNTIME_ERROR;
    }

    while (offset < size) {
        written = fwrite(data + offset, 1u, size - offset, pipe_handle);
        if (written == 0u) {
            if (ferror(pipe_handle)) {
                pclose(pipe_handle);
                sixel_helper_set_additional_message(
                    "clipboard: fwrite() failed.");
                return SIXEL_RUNTIME_ERROR;
            }
            break;
        }
        offset += written;
    }

    close_status = pclose(pipe_handle);
    if (close_status != 0) {
        sixel_helper_set_additional_message(
            "clipboard: helper exited with failure.");
        return SIXEL_BAD_CLIPBOARD;
    }

    return SIXEL_OK;
}

/*
 * Linux clipboard reader that prefers Wayland helpers and gracefully falls
 * back to xclip when X11 is the only option.  The probing order is:
 *
 *   1. wl-paste for PNG or text when both wl-copy and wl-paste are present.
 *   2. xclip as a fallback for either format.
 */
SIXELSTATUS
sixel_clipboard_read_linux(char const *format,
                            unsigned char **data,
                            size_t *size,
                            sixel_allocator_t *allocator)
{
    clipboard_linux_caps_t caps;
    SIXELSTATUS attempt;
    unsigned char *buffer;
    size_t length;
    int want_png;

    caps.probed = 0;
    caps.have_wl_copy = 0;
    caps.have_wl_paste = 0;
    caps.have_xclip = 0;
    attempt = SIXEL_BAD_CLIPBOARD;
    buffer = NULL;
    length = 0u;
    want_png = clipboard_linux_request_png(format);

    if (data == NULL || size == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: destination pointers are null.");
        return SIXEL_BAD_ARGUMENT;
    }

    *data = NULL;
    *size = 0u;

    clipboard_linux_detect_capabilities(&caps);

    if (want_png) {
        if (caps.have_wl_copy && caps.have_wl_paste) {
            attempt = clipboard_linux_read_command(
                clipboard_linux_cmd_wl_paste_png, &buffer, &length,
                allocator);
            if (attempt == SIXEL_OK) {
                *data = buffer;
                *size = length;
                return SIXEL_OK;
            }
            if (attempt != SIXEL_BAD_CLIPBOARD) {
                return attempt;
            }
        }
        if (caps.have_xclip) {
            attempt = clipboard_linux_read_command(
                clipboard_linux_cmd_xclip_read_png, &buffer, &length,
                allocator);
            if (attempt == SIXEL_OK) {
                *data = buffer;
                *size = length;
                return SIXEL_OK;
            }
            return attempt;
        }
        sixel_helper_set_additional_message(
            "clipboard: PNG helper command not available.");
        return SIXEL_BAD_CLIPBOARD;
    }

    if (caps.have_wl_copy && caps.have_wl_paste) {
        attempt = clipboard_linux_read_command(
            clipboard_linux_cmd_wl_paste_text, &buffer, &length, allocator);
        if (attempt == SIXEL_OK) {
            *data = buffer;
            *size = length;
            return SIXEL_OK;
        }
        if (attempt != SIXEL_BAD_CLIPBOARD) {
            return attempt;
        }
    }
    if (caps.have_xclip) {
        attempt = clipboard_linux_read_command(
            clipboard_linux_cmd_xclip_read_text, &buffer, &length, allocator);
        if (attempt == SIXEL_OK) {
            *data = buffer;
            *size = length;
            return SIXEL_OK;
        }
        return attempt;
    }

    sixel_helper_set_additional_message(
        "clipboard: text helper command not available.");
    return SIXEL_BAD_CLIPBOARD;
}

/*
 * Linux clipboard writer mirroring the reader's preference order.  The
 * routine allows callers to fall back to xclip when wl-copy refuses to run
 * (for example, outside of a Wayland session).
 */
SIXELSTATUS
sixel_clipboard_write_linux(char const *format,
                             unsigned char const *data,
                             size_t size)
{
    clipboard_linux_caps_t caps;
    SIXELSTATUS attempt;
    int want_png;

    caps.probed = 0;
    caps.have_wl_copy = 0;
    caps.have_wl_paste = 0;
    caps.have_xclip = 0;
    attempt = SIXEL_BAD_CLIPBOARD;
    want_png = clipboard_linux_request_png(format);

    if (size > 0u && data == NULL) {
        sixel_helper_set_additional_message(
            "clipboard: source buffer is null.");
        return SIXEL_BAD_ARGUMENT;
    }

    clipboard_linux_detect_capabilities(&caps);

    if (want_png) {
        if (caps.have_wl_copy) {
            attempt = clipboard_linux_write_command(
                clipboard_linux_cmd_wl_copy_png, data, size);
            if (attempt == SIXEL_OK) {
                return SIXEL_OK;
            }
            if (attempt != SIXEL_BAD_CLIPBOARD) {
                return attempt;
            }
        }
        if (caps.have_xclip) {
            return clipboard_linux_write_command(
                clipboard_linux_cmd_xclip_write_png, data, size);
        }
        sixel_helper_set_additional_message(
            "clipboard: PNG helper command not available.");
        return SIXEL_BAD_CLIPBOARD;
    }

    if (caps.have_wl_copy) {
        attempt = clipboard_linux_write_command(
            clipboard_linux_cmd_wl_copy_text, data, size);
        if (attempt == SIXEL_OK) {
            return SIXEL_OK;
        }
        if (attempt != SIXEL_BAD_CLIPBOARD) {
            return attempt;
        }
    }
    if (caps.have_xclip) {
        return clipboard_linux_write_command(
            clipboard_linux_cmd_xclip_write_text, data, size);
    }

    sixel_helper_set_additional_message(
        "clipboard: text helper command not available.");
    return SIXEL_BAD_CLIPBOARD;
}

/*
 * Availability probe used by unit tests.  The clipboard is deemed usable when
 * either wl-copy/wl-paste are present or xclip is detected.
 */
int
sixel_clipboard_is_available_linux(void)
{
    clipboard_linux_caps_t caps;

    caps.probed = 0;
    caps.have_wl_copy = 0;
    caps.have_wl_paste = 0;
    caps.have_xclip = 0;

    clipboard_linux_detect_capabilities(&caps);

    if ((caps.have_wl_copy && caps.have_wl_paste) || caps.have_xclip) {
        return 1;
    }

    return 0;
}
#endif

SIXELSTATUS
sixel_clipboard_read(char const *format,
                     unsigned char **data,
                     size_t *size,
                     sixel_allocator_t *allocator)
{
#if defined(HAVE_CLIPBOARD_MACOS)
    return sixel_clipboard_read_macos(format, data, size, allocator);
#elif defined(HAVE_CLIPBOARD_WINDOWS)
    return sixel_clipboard_read_windows(format, data, size, allocator);
#elif defined(HAVE_CLIPBOARD_LINUX)
    return sixel_clipboard_read_linux(format, data, size, allocator);
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
#elif defined(HAVE_CLIPBOARD_WINDOWS)
    return sixel_clipboard_write_windows(format, data, size);
#elif defined(HAVE_CLIPBOARD_LINUX)
    return sixel_clipboard_write_linux(format, data, size);
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
#elif defined(HAVE_CLIPBOARD_WINDOWS)
    return sixel_clipboard_is_available_windows();
#elif defined(HAVE_CLIPBOARD_LINUX)
    return sixel_clipboard_is_available_linux();
#else
    return 0;
#endif
}

