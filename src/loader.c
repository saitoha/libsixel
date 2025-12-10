/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "config.h"
#if !defined(_POSIX_C_SOURCE)
# define _POSIX_C_SOURCE 200809L
#endif

/* STDC_HEADERS */
#include <stdio.h>
#include <stdlib.h>

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_CTYPE_H
# include <ctype.h>
#endif
#if HAVE_STDARG_H
# include <stdarg.h>
#endif
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif
#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif
#if HAVE_FCNTL_H
# include <fcntl.h>
#endif
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#elif HAVE_TIME_H
# include <time.h>
#endif  /* HAVE_SYS_TIME_H HAVE_TIME_H */
#if defined(_WIN32)
# include <windows.h>
#endif
#if HAVE_ERRNO_H
# include <errno.h>
#endif
#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
# include <CoreServices/CoreServices.h>
#endif
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#if HAVE_SPAWN_H
# include <spawn.h>
#endif
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#if HAVE_DIRENT_H
# include <dirent.h>
#endif

#if !defined(HAVE_MEMCPY)
# define memcpy(d, s, n) (bcopy ((s), (d), (n)))
#endif

#include <sixel.h>
#include "loader.h"
#include "loader-builtin.h"
#include "loader-common.h"
#include "loader-coregraphics.h"
#include "loader-gd.h"
#include "loader-gdk-pixbuf2.h"
#include "loader-gnome-thumbnailer.h"
#include "loader-libjpeg.h"
#include "loader-libpng.h"
#include "loader-quicklook.h"
#include "loader-registry.h"
#include "loader-wic.h"
#include "compat_stub.h"
#include "frame.h"
#include "chunk.h"
#include "allocator.h"
#include "assessment.h"
#include "encoder.h"
#include "logger.h"

/*
 * Internal loader state carried across backends.  The fields mirror the
 * original `loader.c` layout to keep statistics, logging, and allocator
 * ownership centralized while implementations move into per-backend files.
 */
struct sixel_loader {
    int ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int loop_control;
    int finsecure;
    int const *cancel_flag;
    void *context;
    sixel_logger_t logger;
    sixel_assessment_t *assessment;
    char *loader_order;
    sixel_allocator_t *allocator;
    char last_loader_name[64];
    char last_source_path[PATH_MAX];
    size_t last_input_bytes;
    int callback_failed;
    int log_loader_finished;
    char log_path[PATH_MAX];
    char log_loader_name[64];
    size_t log_input_bytes;
};

typedef struct sixel_loader_callback_state {
    sixel_loader_t *loader;
    sixel_load_image_function fn;
    void *context;
} sixel_loader_callback_state_t;


#if HAVE_POSIX_SPAWNP
extern char **environ;
#endif

static char *
loader_strdup(char const *text, sixel_allocator_t *allocator)
{
    char *copy;
    size_t length;

    if (text == NULL) {
        return NULL;
    }

    length = strlen(text) + 1;
    copy = (char *)sixel_allocator_malloc(allocator, length);
    if (copy == NULL) {
        return NULL;
    }

#if HAVE_STRCPY_S
    (void)strcpy_s(copy, (rsize_t)(length - 1), text);
#else
    memcpy(copy, text, length);
#endif

    return copy;
}



/*
 * Emit loader stage markers.
 *
 * Loader callbacks run the downstream pipeline synchronously, so the finish
 * marker must be issued before invoking fn_load() to avoid inflating the
 * loader span. The helper keeps the formatting consistent with
 * sixel_encoder_log_stage() without depending on encoder internals.
 */
static void
loader_log_stage(sixel_loader_t *loader,
                 char const *event,
                 char const *fmt,
                 ...)
{
    sixel_logger_t *logger;
    char message[256];
    va_list args;

    logger = NULL;
    if (loader != NULL) {
        logger = &loader->logger;
    }
    if (logger == NULL || logger->file == NULL || !logger->active) {
        return;
    }

    message[0] = '\0';
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
    va_start(args, fmt);
    if (fmt != NULL) {
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
        (void)sixel_compat_vsnprintf(message, sizeof(message), fmt, args);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    }
    va_end(args);
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

    sixel_logger_logf(logger,
                      "worker",
                      "loader",
                      event,
                      -1,
                      -1,
                      0,
                      0,
                      0,
                      0,
                      "%s",
                      message);
}

static SIXELSTATUS
loader_callback_trampoline(sixel_frame_t *frame, void *data)
{
    sixel_loader_callback_state_t *state;
    SIXELSTATUS status;
    sixel_loader_t *loader;

    state = (sixel_loader_callback_state_t *)data;
    loader = NULL;
    if (state == NULL || state->fn == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    loader = state->loader;
    if (loader != NULL && loader->log_loader_finished == 0) {
        loader_log_stage(loader,
                         "finish",
                         "path=%s loader=%s bytes=%zu",
                         loader->log_path,
                         loader->log_loader_name,
                         loader->log_input_bytes);
        loader->log_loader_finished = 1;
    }

    status = state->fn(frame, state->context);
    if (SIXEL_FAILED(status) && state->loader != NULL) {
        state->loader->callback_failed = 1;
    }

    return status;
}

static int
loader_plan_contains(sixel_loader_entry_t const **plan,
                     size_t plan_length,
                     sixel_loader_entry_t const *entry)
{
    size_t index;

    for (index = 0; index < plan_length; ++index) {
        if (plan[index] == entry) {
            return 1;
        }
    }

    return 0;
}

static int
loader_token_matches(char const *token,
                     size_t token_length,
                     char const *name)
{
    size_t index;
    unsigned char left;
    unsigned char right;

    for (index = 0; index < token_length && name[index] != '\0'; ++index) {
        left = (unsigned char)token[index];
        right = (unsigned char)name[index];
        if (tolower(left) != tolower(right)) {
            return 0;
        }
    }

    if (index != token_length || name[index] != '\0') {
        return 0;
    }

    return 1;
}

static sixel_loader_entry_t const *
loader_lookup_token(char const *token,
                    size_t token_length,
                    sixel_loader_entry_t const *entries,
                    size_t entry_count)
{
    size_t index;

    for (index = 0; index < entry_count; ++index) {
        if (loader_token_matches(token,
                                 token_length,
                                 entries[index].name)) {
            return &entries[index];
        }
    }

    return NULL;
}

/*
 * loader_build_plan
 *
 * Translate a comma separated list into an execution plan that reorders the
 * runtime loader chain.  Tokens are matched case-insensitively.  Unknown names
 * are ignored so that new builds remain forward compatible.
 *
 *    user input "gd,coregraphics"
 *                |
 *                v
 *        +-------------------+
 *        | prioritized list  |
 *        +-------------------+
 *                |
 *                v
 *        +-------------------+
 *        | default fallbacks |
 *        +-------------------+
 */
static size_t
loader_build_plan(char const *order,
                  sixel_loader_entry_t const *entries,
                  size_t entry_count,
                  sixel_loader_entry_t const **plan,
                  size_t plan_capacity)
{
    size_t plan_length;
    size_t index;
    char const *cursor;
    char const *token_start;
    char const *token_end;
    size_t token_length;
    sixel_loader_entry_t const *entry;
    size_t limit;

    plan_length = 0;
    index = 0;
    cursor = order;
    token_start = order;
    token_end = order;
    token_length = 0;
    entry = NULL;
    limit = plan_capacity;

    if (order != NULL && plan != NULL && plan_capacity > 0) {
        token_start = order;
        cursor = order;
        while (*cursor != '\0') {
            if (*cursor == ',') {
                token_end = cursor;
                while (token_start < token_end &&
                       isspace((unsigned char)*token_start)) {
                    ++token_start;
                }
                while (token_end > token_start &&
                       isspace((unsigned char)token_end[-1])) {
                    --token_end;
                }
                token_length = (size_t)(token_end - token_start);
                if (token_length > 0) {
                    entry = loader_lookup_token(token_start,
                                                token_length,
                                                entries,
                                                entry_count);
                    if (entry != NULL &&
                        !loader_plan_contains(plan,
                                              plan_length,
                                              entry) &&
                        plan_length < limit) {
                        plan[plan_length] = entry;
                        ++plan_length;
                    }
                }
                token_start = cursor + 1;
            }
            ++cursor;
        }

        token_end = cursor;
        while (token_start < token_end &&
               isspace((unsigned char)*token_start)) {
            ++token_start;
        }
        while (token_end > token_start &&
               isspace((unsigned char)token_end[-1])) {
            --token_end;
        }
        token_length = (size_t)(token_end - token_start);
        if (token_length > 0) {
            entry = loader_lookup_token(token_start,
                                        token_length,
                                        entries,
                                        entry_count);
            if (entry != NULL &&
                !loader_plan_contains(plan, plan_length, entry) &&
                plan_length < limit) {
                plan[plan_length] = entry;
                ++plan_length;
            }
        }
    }

    for (index = 0; index < entry_count && plan_length < limit; ++index) {
        entry = &entries[index];
        if (!entry->default_enabled) {
            continue;
        }
        if (!loader_plan_contains(plan, plan_length, entry)) {
            plan[plan_length] = entry;
            ++plan_length;
        }
    }

    return plan_length;
}

static void
loader_append_chunk(char *dest,
                    size_t capacity,
                    size_t *offset,
                    char const *chunk)
{
    size_t available;
    size_t length;

    if (dest == NULL || offset == NULL || chunk == NULL) {
        return;
    }

    if (*offset >= capacity) {
        return;
    }

    available = capacity - *offset;
    if (available == 0) {
        return;
    }

    length = strlen(chunk);
    if (length >= available) {
        if (available == 0) {
            return;
        }
        length = available - 1u;
    }

    if (length > 0) {
        memcpy(dest + *offset, chunk, length);
        *offset += length;
    }

    if (*offset < capacity) {
        dest[*offset] = '\0';
    } else {
        dest[capacity - 1u] = '\0';
    }
}

static void
loader_append_key_value(char *dest,
                        size_t capacity,
                        size_t *offset,
                        char const *label,
                        char const *value)
{
    char line[128];
    int written;

    if (value == NULL || value[0] == '\0') {
        return;
    }

    written = sixel_compat_snprintf(line,
                                    sizeof(line),
                                    "  %-10s: %s\n",
                                    label,
                                    value);
    if (written < 0) {
        return;
    }

    if ((size_t)written >= sizeof(line)) {
        line[sizeof(line) - 1u] = '\0';
    }

    loader_append_chunk(dest, capacity, offset, line);
}

static void
loader_extract_extension(char const *path, char *buffer, size_t capacity)
{
    char const *dot;
    size_t index;

    if (buffer == NULL || capacity == 0) {
        return;
    }

    buffer[0] = '\0';

    if (path == NULL) {
        return;
    }

    dot = strrchr(path, '.');
    if (dot == NULL || dot[1] == '\0') {
        return;
    }

#if defined(_WIN32)
    {
        char const *slash;
        char const *backslash;

        slash = strrchr(path, '/');
        backslash = strrchr(path, '\\');
        if ((slash != NULL && dot < slash) ||
                (backslash != NULL && dot < backslash)) {
            return;
        }
    }
#else
    {
        char const *slash;

        slash = strrchr(path, '/');
        if (slash != NULL && dot < slash) {
            return;
        }
    }
#endif

    if (dot[1] == '\0') {
        return;
    }

    dot += 1;

    for (index = 0; index + 1 < capacity && dot[index] != '\0'; ++index) {
        buffer[index] = (char)tolower((unsigned char)dot[index]);
    }
    buffer[index] = '\0';
}











#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
static void
loader_copy_cfstring(CFStringRef source, char *buffer, size_t capacity)
{
    if (buffer == NULL || capacity == 0) {
        return;
    }

    buffer[0] = '\0';
    if (source == NULL) {
        return;
    }

    if (!CFStringGetCString(source,
                             buffer,
                             (CFIndex)capacity,
                             kCFStringEncodingUTF8)) {
        buffer[0] = '\0';
    }
}
#endif


static void
loader_publish_diagnostic(sixel_chunk_t const *pchunk,
                          char const *filename)
{
    enum { description_length = 128 };
    enum { uttype_length = 128 };
    enum { extension_length = 32 };
    enum { message_length = 768 };
    char message[message_length];
    char type_value[description_length];
    char extension_text[extension_length + 2];
    char uttype[uttype_length];
    char desc_buffer[description_length];
    char extension[extension_length];
    char const *path;
    char const *display_path;
    char const *metadata_path;
    char const *description_text;
    char *mime_string;
    char *description_string;
    size_t offset;
    int gnome_available;
    int gnome_has_dirs;
    int gnome_has_match;
    int suggestions;

    message[0] = '\0';
    type_value[0] = '\0';
    extension_text[0] = '\0';
    uttype[0] = '\0';
    desc_buffer[0] = '\0';
    extension[0] = '\0';
    path = NULL;
    display_path = "(stdin)";
    metadata_path = NULL;
    description_text = NULL;
    mime_string = NULL;
    description_string = NULL;
    offset = 0u;
    gnome_available = 0;
    gnome_has_dirs = 0;
    gnome_has_match = 0;
    suggestions = 0;

    if (pchunk != NULL && pchunk->source_path != NULL) {
        path = pchunk->source_path;
    } else if (filename != NULL) {
        path = filename;
    }

    if (path != NULL && strcmp(path, "-") != 0) {
        display_path = path;
    }

    if (path != NULL && strcmp(path, "-") != 0 &&
            strstr(path, "://") == NULL) {
        metadata_path = path;
    }

    loader_extract_extension(path, extension, sizeof(extension));

#if HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK
    if (metadata_path != NULL) {
        /*
         * Collect MIME metadata via file(1) when fork() and friends are
         * available.  Windows builds compiled with clang64 lack these
         * interfaces, so the thumbnail helpers remain disabled there.
         */
        mime_string = thumbnailer_guess_content_type(metadata_path);
        description_string = thumbnailer_run_file(metadata_path, NULL);
    }
#else
    (void)metadata_path;
#endif

#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
#if defined(__clang__)
    /*
     * Allow use of legacy UTType C APIs when compiling with the
     * macOS 12 SDK.  The replacement interfaces are Objective-C only,
     * so we must intentionally silence the deprecation warnings here.
     */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    {
        CFStringRef uti_ref;
        CFStringRef mime_ref;
        CFStringRef ext_ref;
        CFStringRef desc_ref;
        CFStringRef preferred_mime;
        char uti_local[uttype_length];
        char desc_local[description_length];
        char mime_local[64];

        uti_ref = NULL;
        mime_ref = NULL;
        ext_ref = NULL;
        desc_ref = NULL;
        preferred_mime = NULL;
        uti_local[0] = '\0';
        desc_local[0] = '\0';
        mime_local[0] = '\0';

        if (mime_string != NULL) {
            mime_ref = CFStringCreateWithCString(kCFAllocatorDefault,
                                                 mime_string,
                                                 kCFStringEncodingUTF8);
        }
        if (mime_ref != NULL) {
            uti_ref = UTTypeCreatePreferredIdentifierForTag(
                kUTTagClassMIMEType,
                mime_ref,
                NULL);
        }
        if (uti_ref == NULL && extension[0] != '\0') {
            ext_ref = CFStringCreateWithCString(kCFAllocatorDefault,
                                                extension,
                                                kCFStringEncodingUTF8);
            if (ext_ref != NULL) {
                uti_ref = UTTypeCreatePreferredIdentifierForTag(
                    kUTTagClassFilenameExtension,
                    ext_ref,
                    NULL);
            }
        }
        if (uti_ref != NULL) {
            loader_copy_cfstring(uti_ref, uti_local, sizeof(uti_local));
            desc_ref = UTTypeCopyDescription(uti_ref);
            if (desc_ref != NULL) {
                loader_copy_cfstring(desc_ref,
                                     desc_local,
                                     sizeof(desc_local));
                CFRelease(desc_ref);
                desc_ref = NULL;
            }
            if (mime_string == NULL) {
                preferred_mime = UTTypeCopyPreferredTagWithClass(
                    uti_ref,
                    kUTTagClassMIMEType);
                if (preferred_mime != NULL) {
                    loader_copy_cfstring(preferred_mime,
                                         mime_local,
                                         sizeof(mime_local));
                    CFRelease(preferred_mime);
                    preferred_mime = NULL;
                }
                if (mime_local[0] != '\0') {
                    mime_string = thumbnailer_strdup(mime_local);
                }
            }
        }
        if (mime_ref != NULL) {
            CFRelease(mime_ref);
        }
        if (ext_ref != NULL) {
            CFRelease(ext_ref);
        }
        if (uti_ref != NULL) {
            CFRelease(uti_ref);
        }
        if (uti_local[0] != '\0') {
            sixel_compat_snprintf(uttype,
                                  sizeof(uttype),
                                  "%s",
                                  uti_local);
        }
        if (desc_local[0] != '\0') {
            sixel_compat_snprintf(desc_buffer,
                                  sizeof(desc_buffer),
                                  "%s",
                                  desc_local);
        }
    }
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

    if (description_string != NULL && description_string[0] != '\0') {
        description_text = description_string;
    } else if (desc_buffer[0] != '\0') {
        description_text = desc_buffer;
    } else {
        description_text = "unknown content";
    }

    sixel_compat_snprintf(type_value,
                          sizeof(type_value),
                          "%s",
                          description_text);

    loader_append_chunk(message,
                        sizeof(message),
                        &offset,
                        "diagnostic:\n");
    loader_append_key_value(message,
                            sizeof(message),
                            &offset,
                            "file",
                            display_path);
    loader_append_key_value(message,
                            sizeof(message),
                            &offset,
                            "type",
                            type_value);

    if (mime_string != NULL && mime_string[0] != '\0') {
        loader_append_key_value(message,
                                sizeof(message),
                                &offset,
                                "mime",
                                mime_string);
    }

    if (uttype[0] != '\0') {
        loader_append_key_value(message,
                                sizeof(message),
                                &offset,
                                "uti",
                                uttype);
    }

    if (extension[0] != '\0') {
        sixel_compat_snprintf(extension_text,
                              sizeof(extension_text),
                              ".%s",
                              extension);
        loader_append_key_value(message,
                                sizeof(message),
                                &offset,
                                "extension",
                                extension_text);
    }

    loader_append_chunk(message,
                        sizeof(message),
                        &offset,
                        "  suggestions:\n");

#if HAVE_COREGRAPHICS && HAVE_QUICKLOOK
    int quicklook_available;
    int quicklook_supported;

    quicklook_available = 0;
    quicklook_supported = 0;

    quicklook_available = loader_registry_entry_available("quicklook");
    if (quicklook_available) {
        quicklook_supported = loader_quicklook_can_decode(pchunk, filename);
    }
    if (quicklook_supported) {
        loader_append_chunk(message,
                            sizeof(message),
                            &offset,
                            "    - QuickLook rendered a preview during "
                            "the probe; try -j quicklook.\n");
        suggestions += 1;
    }
#endif

#if HAVE_UNISTD_H && HAVE_SYS_WAIT_H && HAVE_FORK
    gnome_available = loader_registry_entry_available("gnome-thumbnailer");
    if (gnome_available) {
        loader_probe_gnome_thumbnailers(mime_string,
                                        &gnome_has_dirs,
                                        &gnome_has_match);
        if (gnome_has_dirs && gnome_has_match) {
            loader_append_chunk(message,
                                sizeof(message),
                                &offset,
                                "    - GNOME thumbnailer definitions match "
                                "this MIME type; try -j gnome-thumbnailer.\n"
                                );
            suggestions += 1;
        }
    }
#else
    (void)gnome_available;
    (void)gnome_has_dirs;
    (void)gnome_has_match;
#endif

    if (suggestions == 0) {
        loader_append_chunk(message,
                            sizeof(message),
                            &offset,
                            "    (no thumbnail helper hints)\n");
    }

    if (suggestions > 0) {
        loader_append_chunk(message,
                            sizeof(message),
                            &offset,
                            "  hint       : Enable one of the suggested "
                            "loaders with -j.\n");
    } else {
        loader_append_chunk(message,
                            sizeof(message),
                            &offset,
                            "  hint       : Convert the file to PNG or "
                            "enable optional loaders.\n");
    }

    sixel_helper_set_additional_message(message);

    free(mime_string);
    free(description_string);
}

SIXELAPI SIXELSTATUS
sixel_loader_new(
    sixel_loader_t   /* out */ **pploader,
    sixel_allocator_t/* in */  *allocator)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_loader_t *loader;
    sixel_allocator_t *local_allocator;

    loader = NULL;
    local_allocator = allocator;

    if (pploader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_new: pploader is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (local_allocator == NULL) {
        status = sixel_allocator_new(&local_allocator,
                                     NULL,
                                     NULL,
                                     NULL,
                                     NULL);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        sixel_allocator_ref(local_allocator);
    }

    loader = (sixel_loader_t *)sixel_allocator_malloc(local_allocator,
                                                      sizeof(*loader));
    if (loader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_new: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        sixel_allocator_unref(local_allocator);
        goto end;
    }

    loader->ref = 1;
    loader->fstatic = 0;
    loader->fuse_palette = 0;
    loader->reqcolors = SIXEL_PALETTE_MAX;
    loader->bgcolor[0] = 0;
    loader->bgcolor[1] = 0;
    loader->bgcolor[2] = 0;
    loader->has_bgcolor = 0;
    loader->loop_control = SIXEL_LOOP_AUTO;
    loader->finsecure = 0;
    loader->cancel_flag = NULL;
    loader->context = NULL;
    /*
     * Initialize a private logger. The helper reuses an existing global
     * logger sink when present so loader markers share the timeline with
     * upstream stages without requiring sixel_loader_setopt().
     */
    sixel_logger_init(&loader->logger);
    (void)sixel_logger_prepare_env(&loader->logger);
    loader->assessment = NULL;
    loader->loader_order = NULL;
    loader->allocator = local_allocator;
    loader->last_loader_name[0] = '\0';
    loader->last_source_path[0] = '\0';
    loader->last_input_bytes = 0u;
    loader->callback_failed = 0;
    loader->log_loader_finished = 0;
    loader->log_path[0] = '\0';
    loader->log_loader_name[0] = '\0';
    loader->log_input_bytes = 0u;

    *pploader = loader;
    status = SIXEL_OK;

end:
    return status;
}

SIXELAPI void
sixel_loader_ref(
    sixel_loader_t /* in */ *loader)
{
    if (loader == NULL) {
        return;
    }

    ++loader->ref;
}

SIXELAPI void
sixel_loader_unref(
    sixel_loader_t /* in */ *loader)
{
    sixel_allocator_t *allocator;

    if (loader == NULL) {
        return;
    }

    if (--loader->ref == 0) {
        allocator = loader->allocator;
        sixel_logger_close(&loader->logger);
        sixel_allocator_free(allocator, loader->loader_order);
        sixel_allocator_free(allocator, loader);
        sixel_allocator_unref(allocator);
    }
}

SIXELAPI SIXELSTATUS
sixel_loader_setopt(
    sixel_loader_t /* in */ *loader,
    int            /* in */ option,
    void const     /* in */ *value)
{
    SIXELSTATUS status = SIXEL_FALSE;
    int const *flag;
    unsigned char const *color;
    char const *order;
    char *copy;
    sixel_allocator_t *allocator;

    flag = NULL;
    color = NULL;
    order = NULL;
    copy = NULL;
    allocator = NULL;

    if (loader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_setopt: loader is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end0;
    }

    sixel_loader_ref(loader);

    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        loader->fstatic = flag != NULL ? *flag : 0;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        loader->fuse_palette = flag != NULL ? *flag : 0;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        loader->reqcolors = flag != NULL ? *flag : SIXEL_PALETTE_MAX;
        if (loader->reqcolors < 1) {
            sixel_helper_set_additional_message(
                "sixel_loader_setopt: reqcolors must be 1 or greater.");
            status = SIXEL_BAD_ARGUMENT;
            goto end;
        }
        if (loader->reqcolors > SIXEL_PALETTE_MAX) {
            loader->reqcolors = SIXEL_PALETTE_MAX;
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            loader->has_bgcolor = 0;
        } else {
            color = (unsigned char const *)value;
            loader->bgcolor[0] = color[0];
            loader->bgcolor[1] = color[1];
            loader->bgcolor[2] = color[2];
            loader->has_bgcolor = 1;
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        loader->loop_control = flag != NULL ? *flag : SIXEL_LOOP_AUTO;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_INSECURE:
        flag = (int const *)value;
        loader->finsecure = flag != NULL ? *flag : 0;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_CANCEL_FLAG:
        loader->cancel_flag = (int const *)value;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_LOADER_ORDER:
        allocator = loader->allocator;
        sixel_allocator_free(allocator, loader->loader_order);
        loader->loader_order = NULL;
        if (value != NULL) {
            order = (char const *)value;
            copy = loader_strdup(order, allocator);
            if (copy == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_loader_setopt: loader_strdup() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
            loader->loader_order = copy;
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_CONTEXT:
        loader->context = (void *)value;
        loader->assessment = NULL;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_ASSESSMENT:
        loader->assessment = (sixel_assessment_t *)value;
        status = SIXEL_OK;
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_loader_setopt: unknown option.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

end:
    sixel_loader_unref(loader);

end0:
    return status;
}

SIXELAPI char const *
sixel_loader_get_last_success_name(sixel_loader_t const *loader)
{
    if (loader == NULL || loader->last_loader_name[0] == '\0') {
        return NULL;
    }
    return loader->last_loader_name;
}

SIXELAPI char const *
sixel_loader_get_last_source_path(sixel_loader_t const *loader)
{
    if (loader == NULL || loader->last_source_path[0] == '\0') {
        return NULL;
    }
    return loader->last_source_path;
}

SIXELAPI size_t
sixel_loader_get_last_input_bytes(sixel_loader_t const *loader)
{
    if (loader == NULL) {
        return 0u;
    }
    return loader->last_input_bytes;
}

SIXELAPI SIXELSTATUS
sixel_loader_load_file(
    sixel_loader_t         /* in */ *loader,
    char const             /* in */ *filename,
    sixel_load_image_function /* in */ fn_load)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_chunk_t *pchunk;
    sixel_loader_entry_t const **plan;
    sixel_loader_entry_t const *entries;
    size_t entry_count;
    size_t plan_length;
    size_t plan_index;
    unsigned char *bgcolor;
    int reqcolors;
    sixel_assessment_t *assessment;
    char const *order_override;
    char const *env_order;
    sixel_loader_callback_state_t callback_state;

    pchunk = NULL;
    plan = NULL;
    entries = NULL;
    entry_count = 0;
    plan_length = 0;
    plan_index = 0;
    bgcolor = NULL;
    reqcolors = 0;
    assessment = NULL;
    order_override = NULL;
    env_order = NULL;

    if (loader == NULL) {
        sixel_helper_set_additional_message(
            "sixel_loader_load_file: loader is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end0;
    }

    sixel_loader_ref(loader);

    loader->log_loader_finished = 0;
    loader->log_loader_name[0] = '\0';
    loader->log_input_bytes = 0u;
    loader->log_path[0] = '\0';
    if (filename != NULL) {
        (void)sixel_compat_snprintf(loader->log_path,
                                    sizeof(loader->log_path),
                                    "%s",
                                    filename);
    }
    loader_log_stage(loader, "start", "path=%s", loader->log_path);

    memset(&callback_state, 0, sizeof(callback_state));
    callback_state.loader = loader;
    callback_state.fn = fn_load;
    callback_state.context = loader->context;
    loader->callback_failed = 0;

    entry_count = loader_registry_get_entries(&entries);

    reqcolors = loader->reqcolors;
    if (reqcolors > SIXEL_PALETTE_MAX) {
        reqcolors = SIXEL_PALETTE_MAX;
    }

    assessment = loader->assessment;

    /*
     *  Assessment pipeline sketch:
     *
     *      +-------------+        +--------------+
     *      | chunk read  | -----> | image decode |
     *      +-------------+        +--------------+
     *
     *  The loader owns the hand-off.  Chunk I/O ends before any decoder runs,
     *  so we time the read span in the encoder and pivot to decode once the
     *  chunk is populated.
     */
    status = sixel_chunk_new(&pchunk,
                             filename,
                             loader->finsecure,
                             loader->cancel_flag,
                             loader->allocator);
    if (status != SIXEL_OK) {
        goto end;
    }

    if (pchunk->size == 0 || (pchunk->size == 1 && *pchunk->buffer == '\n')) {
        status = SIXEL_OK;
        goto end;
    }

    if (pchunk->source_path != NULL && pchunk->source_path[0] != '\0') {
        (void)sixel_compat_snprintf(loader->log_path,
                                    sizeof(loader->log_path),
                                    "%s",
                                    pchunk->source_path);
    }

    if (pchunk->buffer == NULL || pchunk->max_size == 0) {
        status = SIXEL_LOGIC_ERROR;
        goto end;
    }

    if (loader->has_bgcolor) {
        bgcolor = loader->bgcolor;
    }

    status = SIXEL_FALSE;
    if (assessment != NULL) {
        sixel_assessment_stage_transition(
            assessment,
            SIXEL_ASSESSMENT_STAGE_IMAGE_DECODE);
    }
    order_override = loader->loader_order;
    /*
     * Honour SIXEL_LOADER_PRIORITY_LIST when callers do not supply
     * a loader order via -L/--loaders or sixel_loader_setopt().
     */
    if (order_override == NULL) {
        env_order = sixel_compat_getenv("SIXEL_LOADER_PRIORITY_LIST");
        if (env_order != NULL && env_order[0] != '\0') {
            order_override = env_order;
        }
    }

    plan = sixel_allocator_malloc(loader->allocator,
                                  entry_count * sizeof(*plan));
    if (plan == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    plan_length = loader_build_plan(order_override,
                                    entries,
                                    entry_count,
                                    plan,
                                    entry_count);

    for (plan_index = 0; plan_index < plan_length; ++plan_index) {
        if (plan[plan_index] == NULL) {
            continue;
        }
        if (plan[plan_index]->predicate != NULL &&
            plan[plan_index]->predicate(pchunk) == 0) {
            continue;
        }
        loader->log_input_bytes = pchunk != NULL ? pchunk->size : 0u;
        if (plan[plan_index]->name != NULL) {
            (void)sixel_compat_snprintf(loader->log_loader_name,
                                        sizeof(loader->log_loader_name),
                                        "%s",
                                        plan[plan_index]->name);
        } else {
            loader->log_loader_name[0] = '\0';
        }
        loader_trace_try(plan[plan_index]->name);
        status = plan[plan_index]->backend(pchunk,
                                           loader->fstatic,
                                           loader->fuse_palette,
                                           reqcolors,
                                           bgcolor,
                                           loader->loop_control,
                                           loader_callback_trampoline,
                                           &callback_state);
        loader_trace_result(plan[plan_index]->name, status);
        if (SIXEL_SUCCEEDED(status)) {
            break;
        }
    }

    if (SIXEL_FAILED(status)) {
        if (!loader->callback_failed &&
                plan_length > 0u &&
                plan_index >= plan_length &&
                pchunk != NULL) {
            status = SIXEL_LOADER_FAILED;
            loader_publish_diagnostic(pchunk, filename);
        }
        goto end;
    }

    if (plan_index < plan_length &&
            plan[plan_index] != NULL &&
            plan[plan_index]->name != NULL) {
        (void)sixel_compat_snprintf(loader->last_loader_name,
                                    sizeof(loader->last_loader_name),
                                    "%s",
                                    plan[plan_index]->name);
    } else {
        loader->last_loader_name[0] = '\0';
    }
    loader->last_input_bytes = pchunk->size;
    if (pchunk->source_path != NULL) {
        size_t path_len;

        path_len = strlen(pchunk->source_path);
        if (path_len >= sizeof(loader->last_source_path)) {
            path_len = sizeof(loader->last_source_path) - 1u;
        }
        memcpy(loader->last_source_path, pchunk->source_path, path_len);
        loader->last_source_path[path_len] = '\0';
    } else {
        loader->last_source_path[0] = '\0';
    }

end:
    if (plan != NULL) {
        sixel_allocator_free(loader->allocator, plan);
        plan = NULL;
    }
    sixel_chunk_destroy(pchunk);
    sixel_loader_unref(loader);

end0:
    return status;
}

/* load image from file */

SIXELAPI SIXELSTATUS
sixel_helper_load_image_file(
    char const                /* in */     *filename,     /* source file name */
    int                       /* in */     fstatic,       /* whether to */
                                                             /* extract a */
                                                             /* static image */
                                                             /* from an */
                                                             /* animated gif */
    int                       /* in */     fuse_palette,  /* whether to */
                                                             /* use a */
                                                             /* paletted */
                                                             /* image; set */
                                                             /* non-zero to */
                                                             /* request one */
    int                       /* in */     reqcolors,     /* requested */
                                                             /* number of */
                                                             /* colors; */
                                                             /* should be */
                                                             /* equal to or */
                                                             /* less than */
                                                             /* SIXEL_ */
                                                             /* PALETTE_ */
                                                             /* MAX */
    unsigned char             /* in */     *bgcolor,      /* background */
                                                             /* color, may */
                                                             /* be NULL */
    int                       /* in */     loop_control,  /* one of enum */
                                                             /* loopControl */
    sixel_load_image_function /* in */     fn_load,       /* callback */
    int                       /* in */     finsecure,     /* true if do */
                                                             /* not verify */
                                                             /* SSL */
    int const                 /* in */     *cancel_flag,  /* cancel flag, */
                                                             /* may be */
                                                             /* NULL */
    void                      /* in/out */ *context,      /* private data */
                                                             /* passed to */
                                                             /* callback */
                                                             /* function, */
                                                             /* may be */
                                                             /* NULL */
    sixel_allocator_t         /* in */     *allocator     /* allocator */
                                                             /* object, */
                                                             /* may be */
                                                             /* NULL */
)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_loader_t *loader;

    loader = NULL;

    status = sixel_loader_new(&loader, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQUIRE_STATIC,
                                 &fstatic);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_USE_PALETTE,
                                 &fuse_palette);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_REQCOLORS,
                                 &reqcolors);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_BGCOLOR,
                                 bgcolor);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_LOOP_CONTROL,
                                 &loop_control);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_INSECURE,
                                 &finsecure);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CANCEL_FLAG,
                                 cancel_flag);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_setopt(loader,
                                 SIXEL_LOADER_OPTION_CONTEXT,
                                 context);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_loader_load_file(loader, filename, fn_load);

end:
    sixel_loader_unref(loader);

    return status;
}


SIXELAPI size_t
sixel_helper_get_available_loader_names(char const **names, size_t max_names)
{
    sixel_loader_entry_t const *entries;
    size_t entry_count;
    size_t limit;
    size_t index;

    entries = NULL;
    entry_count = loader_registry_get_entries(&entries);

    if (names != NULL && max_names > 0) {
        limit = entry_count;
        if (limit > max_names) {
            limit = max_names;
        }
        for (index = 0; index < limit; ++index) {
            names[index] = entries[index].name;
        }
    }

    return entry_count;
}

#if HAVE_TESTS
static sixel_frame_t *test2_saved_frame = NULL;
static int test2_frame_freed = 0;

static void
test2_tracking_free(void *ptr)
{
    if (ptr == (void *)test2_saved_frame) {
        test2_frame_freed = 1;
    }
    free(ptr);
}

static SIXELSTATUS
test2_on_frame(sixel_frame_t *frame, void *context)
{
    (void)context;
    sixel_frame_ref(frame);
    test2_saved_frame = frame;
    return SIXEL_OK;
}

static int
test1(void)
{
    int nret = EXIT_FAILURE;
    unsigned char *ptr = malloc(16);

    nret = EXIT_SUCCESS;
    goto error;

    nret = EXIT_SUCCESS;

error:
    free(ptr);
    return nret;
}

static int
test2(void)
{
    int nret = EXIT_FAILURE;
#if HAVE_GDK_PIXBUF2
    size_t i;
    FILE *fp;
    char const *filename = NULL;
    unsigned char *pixels = NULL;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;
    static char const * const candidates[] = {
        "../images/snake.png",
        "images/snake.png",
        "../../images/snake.png",
    };

    test2_saved_frame = NULL;
    test2_frame_freed = 0;

    for (i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
        fp = fopen(candidates[i], "rb");
        if (fp != NULL) {
            fclose(fp);
            filename = candidates[i];
            break;
        }
    }
    if (filename == NULL) {
        goto end;
    }

    status = sixel_allocator_new(&allocator,
                                 malloc,
                                 calloc,
                                 realloc,
                                 test2_tracking_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_helper_load_image_file(filename,
                                          1,
                                          1,
                                          SIXEL_PALETTE_MAX,
                                          NULL,
                                          SIXEL_LOOP_AUTO,
                                          test2_on_frame,
                                          0,
                                          NULL,
                                          NULL,
                                          allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (test2_saved_frame == NULL || test2_frame_freed) {
        goto end;
    }

    pixels = sixel_frame_get_pixels(test2_saved_frame);
    if (pixels == NULL) {
        goto end;
    }

    sixel_frame_unref(test2_saved_frame);
    test2_saved_frame = NULL;
    if (!test2_frame_freed) {
        goto end;
    }
#endif  /* HAVE_GDK_PIXBUF2 */

    nret = EXIT_SUCCESS;

end:
    if (test2_saved_frame != NULL) {
        sixel_frame_unref(test2_saved_frame);
        test2_saved_frame = NULL;
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return nret;
}


SIXELAPI int
sixel_loader_tests_main(void)
{
    int nret = EXIT_FAILURE;
    size_t i;
    typedef int (* testcase)(void);

    static testcase const testcases[] = {
        test1,
        test2,
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
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
