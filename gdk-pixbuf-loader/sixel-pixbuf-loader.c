/*
 * SPDX-License-Identifier: MIT
 *
 * gdk-pixbuf loader module for SIXEL images.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(HAVE_GDK_PIXBUF2)

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <gdk-pixbuf/gdk-pixbuf-io.h>
#include <gmodule.h>
#include <sixel.h>

#include "sixel_decode_rgba.h"

#define SIXEL_PIXBUF_MAX_SIZE 6000

typedef struct SixelPixbufContext {
    GByteArray *buffer;
    GdkPixbufModuleSizeFunc size_func;
    GdkPixbufModulePreparedFunc prepared_func;
    GdkPixbufModuleUpdatedFunc updated_func;
    gpointer user_data;
} SixelPixbufContext;

static void
sixel_pixbuf_context_free(SixelPixbufContext *context)
{
    if (context == NULL) {
        return;
    }
    if (context->buffer != NULL) {
        g_byte_array_unref(context->buffer);
        context->buffer = NULL;
    }
    g_free(context);
}

static int
sixel_pixbuf_error_code(SIXELSTATUS status)
{
    switch (status) {
    case SIXEL_BAD_ALLOCATION:
        return GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY;
    case SIXEL_BAD_INPUT:
        return GDK_PIXBUF_ERROR_CORRUPT_IMAGE;
    case SIXEL_BAD_ARGUMENT:
        return GDK_PIXBUF_ERROR_FAILED;
    default:
        return GDK_PIXBUF_ERROR_FAILED;
    }
}

static gboolean
sixel_pixbuf_propagate_error(GError **error,
                             SIXELSTATUS status,
                             char const *message)
{
    char const *detail;
    int code;

    code = sixel_pixbuf_error_code(status);
    detail = sixel_helper_format_error(status);
    g_set_error(error,
                GDK_PIXBUF_ERROR,
                code,
                "%s: %s",
                message,
                detail != NULL ? detail : "unknown error");

    return FALSE;
}

#if defined(SIXEL_PIXBUF_LOADER_TESTING)
gboolean
sixel_pixbuf_testing_propagate_error(GError **error,
                                     SIXELSTATUS status,
                                     char const *message)
{
    return sixel_pixbuf_propagate_error(error, status, message);
}

void
sixel_pixbuf_testing_context_free(gpointer context_ptr)
{
    sixel_pixbuf_context_free((SixelPixbufContext *)context_ptr);
}
#endif

static void
sixel_pixbuf_destroy_pixels(guchar *pixels, gpointer data)
{
    (void) data;

    g_free(pixels);
}

static gboolean
sixel_pixbuf_emit_prepared(SixelPixbufContext *context,
                           GdkPixbuf *pixbuf,
                           GError **error)
{
    (void) error;

    if (context->prepared_func != NULL) {
        context->prepared_func(pixbuf, NULL, context->user_data);
    }
    if (context->updated_func != NULL) {
        context->updated_func(pixbuf,
                              0,
                              0,
                              gdk_pixbuf_get_width(pixbuf),
                              gdk_pixbuf_get_height(pixbuf),
                              context->user_data);
    }
    return TRUE;
}

static gpointer
sixel_pixbuf_begin_load(GdkPixbufModuleSizeFunc size_func,
                        GdkPixbufModulePreparedFunc prepared_func,
                        GdkPixbufModuleUpdatedFunc updated_func,
                        gpointer user_data,
                        GError **error)
{
    SixelPixbufContext *context;

    (void) error;

    context = g_new0(SixelPixbufContext, 1U);
    if (context == NULL) {
        return NULL;
    }

    /* Buffer incremental chunks and replay them in stop_load(). */
    context->buffer = g_byte_array_new();
    if (context->buffer == NULL) {
        sixel_pixbuf_context_free(context);
        return NULL;
    }

    context->size_func = size_func;
    context->prepared_func = prepared_func;
    context->updated_func = updated_func;
    context->user_data = user_data;

    return context;
}

static gboolean
sixel_pixbuf_load_increment(gpointer context_ptr,
                            guchar const *buf,
                            guint size,
                            GError **error)
{
    SixelPixbufContext *context;

    (void) error;

    context = (SixelPixbufContext *)context_ptr;
    if (context == NULL || context->buffer == NULL || buf == NULL) {
        return FALSE;
    }

    g_byte_array_append(context->buffer, buf, size);

    return TRUE;
}

static gboolean
sixel_pixbuf_stop_load(gpointer context_ptr, GError **error)
{
    SixelPixbufContext *context;
    unsigned char *pixels;
    int width;
    int height;
    int channels;
    SIXELSTATUS status;
    GdkPixbuf *pixbuf;
    gboolean result;
    gchar const *signature;

    context = (SixelPixbufContext *)context_ptr;
    if (context == NULL || context->buffer == NULL) {
        g_set_error(error,
                    GDK_PIXBUF_ERROR,
                    GDK_PIXBUF_ERROR_FAILED,
                    "sixel loader: invalid context");
        sixel_pixbuf_context_free(context);
        return FALSE;
    }

    pixels = NULL;
    width = 0;
    height = 0;
    channels = 0;
    pixbuf = NULL;
    result = FALSE;
    signature = "\x1bP";

    /*
     * Reject obvious non-SIXEL data before attempting to decode.  The SIXEL
     * stream must begin with an ESC P introducer somewhere in the payload.
     */
    if (context->buffer->len == 0 ||
        g_strstr_len((char const *)context->buffer->data,
                     (gssize)context->buffer->len,
                     signature) == NULL) {
        sixel_pixbuf_context_free(context);
        return sixel_pixbuf_propagate_error(error,
                                            SIXEL_BAD_INPUT,
                                            "sixel loader: missing SIXEL"
                                            " signature");
    }

    /* Decode with black compositing. Missing ST/BEL terminators are
     * tolerated inside sixel_decode_rgba(). */
    status = sixel_decode_rgba(context->buffer->data,
                               context->buffer->len,
                               0,
                               NULL,
                               &pixels,
                               &width,
                               &height,
                               &channels,
                               NULL);
    if (SIXEL_FAILED(status)) {
        sixel_pixbuf_context_free(context);
        return sixel_pixbuf_propagate_error(error,
                                            status,
                                            "sixel loader: decode failed");
    }

    if (width > SIXEL_PIXBUF_MAX_SIZE || height > SIXEL_PIXBUF_MAX_SIZE) {
        g_set_error(error,
                    GDK_PIXBUF_ERROR,
                    GDK_PIXBUF_ERROR_FAILED,
                    "sixel loader: image exceeds 6000x6000 pixels");
        g_free(pixels);
        sixel_pixbuf_context_free(context);
        return FALSE;
    }

    pixbuf = gdk_pixbuf_new_from_data(pixels,
                                      GDK_COLORSPACE_RGB,
                                      channels == 4 ? TRUE : FALSE,
                                      8,
                                      width,
                                      height,
                                      width * channels,
                                      sixel_pixbuf_destroy_pixels,
                                      pixels);
    if (pixbuf == NULL) {
        g_free(pixels);
        sixel_pixbuf_context_free(context);
        g_set_error(error,
                    GDK_PIXBUF_ERROR,
                    GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                    "sixel loader: pixbuf allocation failed");
        return FALSE;
    }

    if (context->size_func != NULL) {
        context->size_func(&width, &height, context->user_data);
    }

    /* Notify the caller only after the pixbuf is fully constructed. */
    result = sixel_pixbuf_emit_prepared(context, pixbuf, error);

    g_object_unref(pixbuf);
    sixel_pixbuf_context_free(context);

    return result;
}

G_MODULE_EXPORT void
fill_vtable(GdkPixbufModule *module)
{
    module->begin_load = sixel_pixbuf_begin_load;
    module->stop_load = sixel_pixbuf_stop_load;
    module->load_increment = sixel_pixbuf_load_increment;
    module->load = NULL;
    module->save = NULL;
    module->load_animation = NULL;
}

G_MODULE_EXPORT void
fill_info(GdkPixbufFormat *info)
{
    /*
     * Mask bytes use spaces for exact matching. The leading '*' unanchors
     * the second pattern so ESC P is detected even when the introducer is
     * preceded by control codes.
     */
    static GdkPixbufModulePattern signature[] = {
        { "\x1bPq", "   ", 100 }, /* anchored ESC P q */
        { "\x1bP", "* ", 90 },    /* unanchored ESC P */
        { NULL, NULL, 0 }
    };
    static gchar *mime_types[] = {
        "image/x-sixel",
        "image/sixel",
        NULL
    };
    static gchar *extensions[] = {
        "sixel",
        "six",
        NULL
    };

    info->name = "sixel";
    info->signature = signature;
    info->description = "libsixel SIXEL loader";
    info->mime_types = mime_types;
    info->extensions = extensions;
    info->flags = GDK_PIXBUF_FORMAT_THREADSAFE;
    info->license = "MIT";
}

#else

#ifndef G_MODULE_EXPORT
#define G_MODULE_EXPORT
#endif

typedef struct _GdkPixbufModule GdkPixbufModule;
typedef struct _GdkPixbufFormat GdkPixbufFormat;

G_MODULE_EXPORT void
fill_vtable(GdkPixbufModule *module)
{
    (void) module;
}

G_MODULE_EXPORT void
fill_info(GdkPixbufFormat *info)
{
    (void) info;
}

#endif
