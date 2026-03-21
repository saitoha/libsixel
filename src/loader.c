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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
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
# if !defined(UNICODE)
#  define UNICODE
# endif
# if !defined(_UNICODE)
#  define _UNICODE
# endif
# if !defined(WIN32_LEAN_AND_MEAN)
#  define WIN32_LEAN_AND_MEAN
# endif
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
#include "loader-librsvg.h"
#include "loader-order-schema.h"
#include "loader-quicklook.h"
#include "loader-registry.h"
#include "loader-factory.h"
#include "loader-manager.h"
#include "loader-component.h"
#include "loader-wic.h"
#include "compat_stub.h"
#include "frame.h"
#include "chunk.h"
#include "allocator.h"
#include "encoder.h"
#include "logger.h"
#include "options.h"
#include "sixel_atomic.h"

/*
 * Internal loader state carried across backends.  The fields mirror the
 * original `loader.c` layout to keep statistics, logging, and allocator
 * ownership centralized while implementations move into per-backend files.
 */
struct sixel_loader {
    sixel_atomic_u32_t ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int loop_control;
    int finsecure;
    int has_start_frame_no;
    int start_frame_no;
    int const *cancel_flag;
    void *context;
    sixel_logger_t logger;
    char *loader_order;
    sixel_option_argument_list_resolution_t loader_order_resolution;
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

typedef struct sixel_loader_component_option_context {
    sixel_loader_t *loader;
    int reqcolors;
} sixel_loader_component_option_context_t;

typedef struct sixel_loader_manager_trace_context {
    sixel_loader_t *loader;
    size_t input_bytes;
} sixel_loader_manager_trace_context_t;

int
sixel_loader_callback_is_canceled(void *data)
{
    sixel_loader_callback_state_t *state;

    state = (sixel_loader_callback_state_t *)data;
    if (state == NULL || state->loader == NULL ||
        state->loader->cancel_flag == NULL) {
        return 0;
    }

    return *state->loader->cancel_flag != 0;
}


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

    /* Copy the terminating NUL byte as part of length. */
    memcpy(copy, text, length);

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
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wformat-nonliteral"
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-nonliteral"
# endif
#endif
    va_start(args, fmt);
    if (fmt != NULL) {
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wformat-nonliteral"
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat-nonliteral"
# endif
#endif
        (void)sixel_compat_vsnprintf(message, sizeof(message), fmt, args);
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
#endif
    }
    va_end(args);
#if HAVE_DIAGNOSTIC_FORMAT_NONLITERAL
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__) && !defined(__PCC__)
#  pragma GCC diagnostic pop
# endif
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


static SIXELSTATUS
loader_apply_component_options(sixel_loader_component_t *component,
                               sixel_loader_t const *loader,
                               int reqcolors)
{
    typedef struct loader_component_option_entry {
        int option;
        char const *name;
    } loader_component_option_entry_t;

    loader_component_option_entry_t const options[] = {
        { SIXEL_LOADER_OPTION_REQUIRE_STATIC, "require-static" },
        { SIXEL_LOADER_OPTION_USE_PALETTE, "use-palette" },
        { SIXEL_LOADER_OPTION_REQCOLORS, "reqcolors" },
        { SIXEL_LOADER_OPTION_BGCOLOR, "bgcolor" },
        { SIXEL_LOADER_OPTION_LOOP_CONTROL, "loop-control" },
        { SIXEL_LOADER_OPTION_START_FRAME_NO, "start-frame-no" }
    };
    void const *value;
    char message[128];
    size_t index;
    SIXELSTATUS status;

    /*
     * Distribute common execution parameters to every loader component.
     *
     * +---------------------------+-------------------------------+
     * | option                    | value source                  |
     * +---------------------------+-------------------------------+
     * | REQUIRE_STATIC            | loader->fstatic               |
     * | USE_PALETTE               | loader->fuse_palette          |
     * | REQCOLORS                 | normalized reqcolors          |
     * | BGCOLOR                   | loader->bgcolor or NULL       |
     * | LOOP_CONTROL              | loader->loop_control          |
     * | START_FRAME_NO            | loader->start_frame_no/NULL   |
     * +---------------------------+-------------------------------+
     */
    status = SIXEL_OK;
    message[0] = '\0';
    index = 0;
    value = NULL;
    if (component == NULL || loader == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    for (index = 0; index < sizeof(options) / sizeof(options[0]); ++index) {
        switch (options[index].option) {
        case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
            value = &loader->fstatic;
            break;
        case SIXEL_LOADER_OPTION_USE_PALETTE:
            value = &loader->fuse_palette;
            break;
        case SIXEL_LOADER_OPTION_REQCOLORS:
            value = &reqcolors;
            break;
        case SIXEL_LOADER_OPTION_BGCOLOR:
            value = loader->has_bgcolor ? loader->bgcolor : NULL;
            break;
        case SIXEL_LOADER_OPTION_LOOP_CONTROL:
            value = &loader->loop_control;
            break;
        case SIXEL_LOADER_OPTION_START_FRAME_NO:
            value = loader->has_start_frame_no
                ? &loader->start_frame_no : NULL;
            break;
        default:
            value = NULL;
            break;
        }

        status = sixel_loader_component_setopt(component,
                                               options[index].option,
                                               value);
        if (SIXEL_FAILED(status)) {
            (void)sixel_compat_snprintf(message,
                                        sizeof(message),
                                        "sixel_loader_load_file: "
                                        "failed to apply loader option "
                                        "'%s'.",
                                        options[index].name);
            sixel_helper_set_additional_message(
                message);
            return status;
        }
    }

    return SIXEL_OK;
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

#if HAVE_FREEDESKTOP_THUMBNAILING
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
                            "the probe; try -L quicklook.\n");
        suggestions += 1;
    }
#endif

#if HAVE_FREEDESKTOP_THUMBNAILING
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
                                "this MIME type; try -L gnome-thumbnailer.\n"
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
                            "loaders with -L.\n");
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

    loader->ref = 1U;
    loader->fstatic = 0;
    /*
     * Default policy: keep source palettes when a loader can provide them.
     * This avoids unnecessary requantization loss in downstream encoding.
     */
    loader->fuse_palette = 1;
    loader->reqcolors = SIXEL_PALETTE_MAX;
    loader->bgcolor[0] = 0;
    loader->bgcolor[1] = 0;
    loader->bgcolor[2] = 0;
    loader->has_bgcolor = 0;
    loader->loop_control = SIXEL_LOOP_AUTO;
    loader->finsecure = 0;
    loader->has_start_frame_no = 0;
    loader->start_frame_no = INT_MIN;
    loader->cancel_flag = NULL;
    loader->context = NULL;
    /*
     * Initialize a private logger. The helper reuses an existing global
     * logger sink when present so loader markers share the timeline with
     * upstream stages without requiring sixel_loader_setopt().
     */
    sixel_logger_init(&loader->logger);
    (void)sixel_logger_prepare_env(&loader->logger);
    loader->loader_order = NULL;
    loader->loader_order_resolution.canonical_argument = NULL;
    loader->loader_order_resolution.has_trailing_bang = 0;
    loader->loader_order_resolution.items = NULL;
    loader->loader_order_resolution.item_count = 0u;
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

    (void)sixel_atomic_fetch_add_u32(&loader->ref, 1U);
}

SIXELAPI void
sixel_loader_unref(
    sixel_loader_t /* in */ *loader)
{
    sixel_allocator_t *allocator;
    unsigned int previous;

    if (loader == NULL) {
        return;
    }

    previous = sixel_atomic_fetch_sub_u32(&loader->ref, 1U);
    if (previous == 1U) {
        allocator = loader->allocator;
        sixel_logger_close(&loader->logger);
        sixel_allocator_free(allocator, loader->loader_order);
        sixel_option_free_argument_list_resolution(
            &loader->loader_order_resolution);
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
    char const *canonical_order;
    char *copy;
    sixel_allocator_t *allocator;
    char match_detail[128];
    sixel_option_argument_list_resolution_t parsed_order;

    flag = NULL;
    color = NULL;
    order = NULL;
    canonical_order = NULL;
    copy = NULL;
    allocator = NULL;
    match_detail[0] = '\0';
    parsed_order.canonical_argument = NULL;
    parsed_order.has_trailing_bang = 0;
    parsed_order.items = NULL;
    parsed_order.item_count = 0u;

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
        loader->fuse_palette = flag != NULL ? *flag : 1;
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
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            loader->has_start_frame_no = 0;
            loader->start_frame_no = INT_MIN;
        } else {
            flag = (int const *)value;
            loader->start_frame_no = *flag;
            loader->has_start_frame_no = 1;
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_CANCEL_FLAG:
        loader->cancel_flag = (int const *)value;
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_LOADER_ORDER:
        allocator = loader->allocator;
        if (value != NULL) {
            order = (char const *)value;
            status = sixel_loader_order_parse_and_validate(order,
                                                           &parsed_order,
                                                           match_detail,
                                                           sizeof(match_detail));
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            canonical_order = parsed_order.canonical_argument;
            if (canonical_order == NULL) {
                canonical_order = "";
            }
            copy = loader_strdup(canonical_order, allocator);
            if (copy == NULL) {
                sixel_helper_set_additional_message(
                    "sixel_loader_setopt: loader_strdup() failed.");
                status = SIXEL_BAD_ALLOCATION;
                goto end;
            }
        }
        sixel_allocator_free(allocator, loader->loader_order);
        loader->loader_order = copy;
        copy = NULL;
        sixel_option_free_argument_list_resolution(&loader->loader_order_resolution);
        if (value != NULL) {
            loader->loader_order_resolution = parsed_order;
            parsed_order.canonical_argument = NULL;
            parsed_order.has_trailing_bang = 0;
            parsed_order.items = NULL;
            parsed_order.item_count = 0u;
        }
        status = SIXEL_OK;
        break;
    case SIXEL_LOADER_OPTION_CONTEXT:
        loader->context = (void *)value;
        status = SIXEL_OK;
        break;
    default:
        sixel_helper_set_additional_message(
            "sixel_loader_setopt: unknown option.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

end:
    if (copy != NULL) {
        sixel_allocator_free(loader->allocator, copy);
    }
    sixel_option_free_argument_list_resolution(&parsed_order);
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

int
sixel_loader_get_start_frame_no(sixel_loader_t const *loader,
                                int *start_frame_no)
{
    if (start_frame_no != NULL) {
        *start_frame_no = INT_MIN;
    }
    if (loader == NULL || start_frame_no == NULL ||
        loader->has_start_frame_no == 0) {
        return 0;
    }

    *start_frame_no = loader->start_frame_no;
    return 1;
}

static SIXELSTATUS
loader_manager_configure_component(sixel_loader_component_t *component,
                                   void *context)
{
    sixel_loader_component_option_context_t *options;

    options = (sixel_loader_component_option_context_t *)context;
    if (options == NULL || options->loader == NULL) {
        sixel_helper_set_additional_message(
            "loader_manager_configure_component: invalid context.");
        return SIXEL_BAD_ARGUMENT;
    }

    return loader_apply_component_options(component,
                                          options->loader,
                                          options->reqcolors);
}

static void
loader_manager_trace_try_callback(char const *name, void *context)
{
    sixel_loader_manager_trace_context_t *trace;

    trace = (sixel_loader_manager_trace_context_t *)context;
    if (trace == NULL || trace->loader == NULL) {
        return;
    }

    trace->loader->log_input_bytes = trace->input_bytes;
    if (name != NULL) {
        (void)sixel_compat_snprintf(trace->loader->log_loader_name,
                                    sizeof(trace->loader->log_loader_name),
                                    "%s",
                                    name);
    } else {
        trace->loader->log_loader_name[0] = '\0';
    }
    loader_trace_try(name);
}

static void
loader_manager_trace_result_callback(char const *name,
                                     SIXELSTATUS status,
                                     void *context)
{
    (void)context;
    loader_trace_result(name, status);
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
    sixel_loader_factory_t *factory;
    sixel_loader_manager_t *manager;
    sixel_loader_chain_t *chain;
    size_t entry_count;
    size_t plan_length;
    int reqcolors;
    char const *order_override;
    char const *plan_order;
    char const *env_order;
    sixel_option_argument_list_resolution_t const *active_order_resolution;
    char const *selected_name;
    sixel_loader_callback_state_t callback_state;
    sixel_loader_component_option_context_t option_context;
    sixel_loader_manager_trace_context_t trace_context;
    sixel_option_argument_list_resolution_t order_resolution;

    pchunk = NULL;
    plan = NULL;
    entries = NULL;
    factory = NULL;
    manager = NULL;
    chain = NULL;
    entry_count = 0;
    plan_length = 0;
    reqcolors = 0;
    order_override = NULL;
    plan_order = NULL;
    env_order = NULL;
    active_order_resolution = NULL;
    selected_name = NULL;
    order_resolution.canonical_argument = NULL;
    order_resolution.has_trailing_bang = 0;
    order_resolution.items = NULL;
    order_resolution.item_count = 0u;
    memset(&option_context, 0, sizeof(option_context));
    memset(&trace_context, 0, sizeof(trace_context));

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

    status = loader_factory_get_default(&factory);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    entry_count = loader_factory_get_entries(factory, &entries);

    reqcolors = loader->reqcolors;
    if (reqcolors > SIXEL_PALETTE_MAX) {
        reqcolors = SIXEL_PALETTE_MAX;
    }

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

    status = SIXEL_FALSE;
    order_override = loader->loader_order;
    if (order_override == NULL) {
        env_order = sixel_compat_getenv("SIXEL_LOADER_PRIORITY_LIST");
        if (env_order != NULL && env_order[0] != '\0') {
            order_override = env_order;
        }
    }
    plan_order = order_override;
    if (order_override != NULL && order_override[0] != '\0') {
        if (order_override == loader->loader_order) {
            active_order_resolution = &loader->loader_order_resolution;
            plan_order = loader->loader_order_resolution.canonical_argument;
        } else {
            status = loader_manager_parse_loader_order(order_override,
                                                       &order_resolution);
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            active_order_resolution = &order_resolution;
            plan_order = order_resolution.canonical_argument;
        }
    }
    loader_manager_apply_loader_suboptions_resolution(active_order_resolution);

    plan = sixel_allocator_malloc(loader->allocator,
                                  entry_count * sizeof(*plan));
    if (plan == NULL) {
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }

    if (active_order_resolution != NULL) {
        plan_length = loader_manager_build_plan_from_resolution(active_order_resolution,
                                                                entries,
                                                                entry_count,
                                                                plan,
                                                                entry_count);
    } else {
        plan_length = loader_manager_build_plan(plan_order,
                                                entries,
                                                entry_count,
                                                plan,
                                                entry_count);
    }
    if (plan_length == 0u) {
        if (plan_order != NULL && plan_order[0] != '\0') {
            sixel_helper_set_additional_message(
                "sixel_loader_load_file: no supported loader in loader "
                "order.");
            status = SIXEL_BAD_ARGUMENT;
        } else {
            sixel_helper_set_additional_message(
                "sixel_loader_load_file: no available loader backend.");
            status = SIXEL_LOADER_FAILED;
        }
        goto end;
    }

    status = loader_manager_get_default(&manager);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = loader_manager_build_chain_from_plan(manager,
                                                  plan,
                                                  plan_length,
                                                  pchunk,
                                                  loader->allocator,
                                                  &chain);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    option_context.loader = loader;
    option_context.reqcolors = reqcolors;
    trace_context.loader = loader;
    trace_context.input_bytes = pchunk->size;
    status = loader_manager_execute_chain(
        manager,
        chain,
        pchunk,
        loader_callback_trampoline,
        &callback_state,
        loader_manager_configure_component,
        &option_context,
        loader_manager_trace_try_callback,
        loader_manager_trace_result_callback,
        &trace_context,
        &selected_name);

    if (SIXEL_FAILED(status)) {
        if (status == SIXEL_FALSE) {
            if (!loader->callback_failed && pchunk != NULL) {
                status = SIXEL_LOADER_FAILED;
                loader_publish_diagnostic(pchunk, filename);
            } else {
                sixel_helper_set_additional_message(
                    "sixel_loader_load_file: loader returned "
                    "unspecified failure.");
                status = SIXEL_LOADER_FAILED;
            }
        }
        goto end;
    }

    if (selected_name != NULL) {
        (void)sixel_compat_snprintf(loader->last_loader_name,
                                    sizeof(loader->last_loader_name),
                                    "%s",
                                    selected_name);
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
    loader_chain_unref(chain);
    chain = NULL;
    loader_manager_unref(manager);
    manager = NULL;
    if (plan != NULL) {
        sixel_allocator_free(loader->allocator, plan);
        plan = NULL;
    }
    loader_factory_unref(factory);
    factory = NULL;
    sixel_chunk_destroy(pchunk);
    sixel_option_free_argument_list_resolution(&order_resolution);
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
    sixel_loader_factory_t *factory;
    size_t entry_count;
    size_t limit;
    size_t index;
    SIXELSTATUS status;

    entries = NULL;
    factory = NULL;
    entry_count = 0u;
    limit = 0u;
    index = 0u;
    status = SIXEL_FALSE;

    status = loader_factory_get_default(&factory);
    if (SIXEL_FAILED(status)) {
        return 0u;
    }
    entry_count = loader_factory_get_entries(factory, &entries);

    if (names != NULL && max_names > 0) {
        limit = entry_count;
        if (limit > max_names) {
            limit = max_names;
        }
        for (index = 0; index < limit; ++index) {
            names[index] = entries[index].name;
        }
    }

    loader_factory_unref(factory);

    return entry_count;
}


/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
