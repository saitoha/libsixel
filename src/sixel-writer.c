/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 libsixel developers. See `AUTHORS`.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
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

#include <sixel.h>

#if HAVE_ERRNO_H
# include <errno.h>
#endif
#include <stdio.h>
#include <string.h>

#include "sixel-writer.h"
#include "sixel_atomic.h"
#include "threading.h"

#define DCS_START_7BIT       "\033P"
#define DCS_START_7BIT_SIZE  (sizeof(DCS_START_7BIT) - 1)
#define DCS_START_8BIT       "\220"
#define DCS_START_8BIT_SIZE  (sizeof(DCS_START_8BIT) - 1)
#define DCS_END_7BIT         "\033\\"
#define DCS_END_7BIT_SIZE    (sizeof(DCS_END_7BIT) - 1)
#define DCS_END_8BIT         "\234"
#define DCS_END_8BIT_SIZE    (sizeof(DCS_END_8BIT) - 1)
#define DCS_7BIT(x)          DCS_START_7BIT x DCS_END_7BIT
#define SCREEN_PACKET_SIZE   256

typedef struct sixel_writer_storage {
    sixel_writer_t interface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_write_function fn_write;
    void *priv;
    sixel_writer_controls_t controls;
    sixel_mutex_t mutex;
    int used_penetration;
    int mutex_ready;
} sixel_writer_storage_t;

static sixel_writer_storage_t *
sixel_writer_storage_from_interface(sixel_writer_t *self);
static unsigned char
sixel_writer_flag_to_byte(int value);
static void
sixel_writer_lock(sixel_writer_storage_t *storage);
static void
sixel_writer_unlock(sixel_writer_storage_t *storage);
static SIXELSTATUS
sixel_writer_write_raw_locked(sixel_writer_storage_t *storage,
                              char const *data,
                              int size);
static SIXELSTATUS
sixel_writer_write_penetrated_locked(sixel_writer_storage_t *storage,
                                     char const *data,
                                     int size);
static SIXELSTATUS
sixel_writer_vtbl_begin_image(sixel_writer_t *self,
                              sixel_writer_image_header_t const *header);
static SIXELSTATUS
sixel_writer_vtbl_write(sixel_writer_t *self, char const *data, int size);
static SIXELSTATUS
sixel_writer_vtbl_end_image(sixel_writer_t *self);
static void
sixel_writer_vtbl_ref(sixel_writer_t *self);
static void
sixel_writer_vtbl_unref(sixel_writer_t *self);
static SIXELSTATUS
sixel_writer_vtbl_init(sixel_writer_t *self,
                       sixel_writer_init_request_t const *request);
static SIXELSTATUS
sixel_writer_vtbl_set_controls(sixel_writer_t *self,
                               sixel_writer_controls_t const *controls);
static SIXELSTATUS
sixel_writer_vtbl_get_controls(sixel_writer_t *self,
                               sixel_writer_controls_t *controls);

static sixel_writer_vtbl_t const g_sixel_writer_vtbl = {
    sixel_writer_vtbl_ref,
    sixel_writer_vtbl_unref,
    sixel_writer_vtbl_init,
    sixel_writer_vtbl_set_controls,
    sixel_writer_vtbl_get_controls,
    sixel_writer_vtbl_begin_image,
    sixel_writer_vtbl_write,
    sixel_writer_vtbl_end_image
};

static sixel_writer_storage_t *
sixel_writer_storage_from_interface(sixel_writer_t *self)
{
    return (sixel_writer_storage_t *)(void *)self;
}

static unsigned char
sixel_writer_flag_to_byte(int value)
{
    unsigned char flag;

    flag = (unsigned char)(value != 0 ? 1 : 0);

    return flag;
}

static void
sixel_writer_lock(sixel_writer_storage_t *storage)
{
    if (storage != NULL && storage->mutex_ready) {
        sixel_mutex_lock(&storage->mutex);
    }
}

static void
sixel_writer_unlock(sixel_writer_storage_t *storage)
{
    if (storage != NULL && storage->mutex_ready) {
        sixel_mutex_unlock(&storage->mutex);
    }
}

static SIXELSTATUS
sixel_writer_write_raw_locked(sixel_writer_storage_t *storage,
                              char const *data,
                              int size)
{
    int result;

    if (storage == NULL || data == NULL || size < 0) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (size == 0) {
        return SIXEL_OK;
    }
    if (storage->fn_write == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    /*
     * The callback is intentionally serialized by the writer lock held by the
     * caller.  Re-entering the same writer from the callback would deadlock in
     * thread-enabled builds and is outside the component contract.
     */
    result = storage->fn_write((char *)data, size, storage->priv);
    if (result < 0) {
#if HAVE_ERRNO_H
        return (SIXEL_LIBC_ERROR | (errno & 0xff));
#else
        return SIXEL_LIBC_ERROR;
#endif
    }

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_writer_write_penetrated_locked(sixel_writer_storage_t *storage,
                                     char const *data,
                                     int size)
{
    SIXELSTATUS status;
    int pos;
    int chunk_size;
    int split_size;

    if (storage == NULL || data == NULL || size < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    status = SIXEL_OK;
    split_size = SCREEN_PACKET_SIZE
        - DCS_START_7BIT_SIZE - DCS_END_7BIT_SIZE;
    if (size > 0) {
        storage->used_penetration = 1;
    }
    for (pos = 0; pos < size; pos += split_size) {
        chunk_size = size - pos;
        if (chunk_size > split_size) {
            chunk_size = split_size;
        }
        status = sixel_writer_write_raw_locked(storage,
                                               DCS_START_7BIT,
                                               DCS_START_7BIT_SIZE);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        status = sixel_writer_write_raw_locked(storage,
                                               data + pos,
                                               chunk_size);
        if (SIXEL_FAILED(status)) {
            return status;
        }
        status = sixel_writer_write_raw_locked(storage,
                                               DCS_END_7BIT,
                                               DCS_END_7BIT_SIZE);
        if (SIXEL_FAILED(status)) {
            return status;
        }
    }

    return status;
}

static void
sixel_writer_vtbl_ref(sixel_writer_t *self)
{
    sixel_writer_storage_t *storage;

    if (self == NULL) {
        return;
    }

    storage = sixel_writer_storage_from_interface(self);
    (void)sixel_atomic_fetch_add_u32(&storage->ref, 1U);
}

static void
sixel_writer_vtbl_unref(sixel_writer_t *self)
{
    sixel_writer_storage_t *storage;
    sixel_allocator_t *allocator;
    unsigned int previous;

    if (self == NULL) {
        return;
    }

    storage = sixel_writer_storage_from_interface(self);
    previous = sixel_atomic_fetch_sub_u32(&storage->ref, 1U);
    if (previous != 1U) {
        return;
    }

    if (storage->mutex_ready) {
        sixel_mutex_destroy(&storage->mutex);
    }
    allocator = storage->allocator;
    sixel_allocator_free(allocator, storage);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_writer_vtbl_init(sixel_writer_t *self,
                       sixel_writer_init_request_t const *request)
{
    sixel_writer_storage_t *storage;

    if (self == NULL || request == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_writer_storage_from_interface(self);
    storage->fn_write = request->fn_write;
    storage->priv = request->priv;

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_writer_vtbl_set_controls(sixel_writer_t *self,
                               sixel_writer_controls_t const *controls)
{
    sixel_writer_storage_t *storage;

    if (self == NULL || controls == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_writer_storage_from_interface(self);
    sixel_writer_lock(storage);
    storage->controls.has_8bit_control =
        sixel_writer_flag_to_byte(controls->has_8bit_control);
    storage->controls.has_sixel_scrolling =
        sixel_writer_flag_to_byte(controls->has_sixel_scrolling);
    storage->controls.has_sdm_glitch =
        sixel_writer_flag_to_byte(controls->has_sdm_glitch);
    storage->controls.skip_dcs_envelope =
        sixel_writer_flag_to_byte(controls->skip_dcs_envelope);
    storage->controls.skip_header =
        sixel_writer_flag_to_byte(controls->skip_header);
    storage->controls.penetrate_multiplexer =
        sixel_writer_flag_to_byte(controls->penetrate_multiplexer);
    sixel_writer_unlock(storage);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_writer_vtbl_get_controls(sixel_writer_t *self,
                               sixel_writer_controls_t *controls)
{
    sixel_writer_storage_t *storage;

    if (self == NULL || controls == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_writer_storage_from_interface(self);
    sixel_writer_lock(storage);
    *controls = storage->controls;
    sixel_writer_unlock(storage);

    return SIXEL_OK;
}

static SIXELSTATUS
sixel_writer_vtbl_begin_image(sixel_writer_t *self,
                              sixel_writer_image_header_t const *header)
{
    SIXELSTATUS status;
    sixel_writer_storage_t *storage;
    char buffer[128];
    int nwrite;

    if (self == NULL || header == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_writer_storage_from_interface(self);
    nwrite = 0;

    if (!storage->controls.skip_header) {
        if (header->parameter_count == 3) {
            nwrite = snprintf(buffer, sizeof(buffer), "%d;%d;%dq",
                              header->parameter0,
                              header->parameter1,
                              header->parameter2);
        } else if (header->parameter_count == 2) {
            nwrite = snprintf(buffer, sizeof(buffer), "%d;%dq",
                              header->parameter0,
                              header->parameter1);
        } else if (header->parameter_count == 1) {
            nwrite = snprintf(buffer, sizeof(buffer), "%dq",
                              header->parameter0);
        } else {
            nwrite = snprintf(buffer, sizeof(buffer), "q");
        }
        if (nwrite < 0 || nwrite >= (int)sizeof(buffer)) {
            return SIXEL_RUNTIME_ERROR;
        }
    }

    sixel_writer_lock(storage);
    storage->used_penetration = 0;
    if (!storage->controls.skip_dcs_envelope &&
        !storage->controls.penetrate_multiplexer) {
        if (storage->controls.has_8bit_control) {
            status = sixel_writer_write_raw_locked(storage,
                                                   DCS_START_8BIT,
                                                   DCS_START_8BIT_SIZE);
        } else {
            status = sixel_writer_write_raw_locked(storage,
                                                   DCS_START_7BIT,
                                                   DCS_START_7BIT_SIZE);
        }
        if (SIXEL_FAILED(status)) {
            sixel_writer_unlock(storage);
            return status;
        }
    }

    if (!storage->controls.skip_header) {
        status = storage->controls.penetrate_multiplexer
            ? sixel_writer_write_penetrated_locked(storage, buffer, nwrite)
            : sixel_writer_write_raw_locked(storage, buffer, nwrite);
        if (SIXEL_FAILED(status)) {
            sixel_writer_unlock(storage);
            return status;
        }
    }
    if (header->use_raster_attributes) {
        nwrite = snprintf(buffer, sizeof(buffer), "\"1;1;%d;%d",
                          header->width,
                          header->height);
        if (nwrite < 0 || nwrite >= (int)sizeof(buffer)) {
            sixel_writer_unlock(storage);
            return SIXEL_RUNTIME_ERROR;
        }
        status = storage->controls.penetrate_multiplexer
            ? sixel_writer_write_penetrated_locked(storage, buffer, nwrite)
            : sixel_writer_write_raw_locked(storage, buffer, nwrite);
        if (SIXEL_FAILED(status)) {
            sixel_writer_unlock(storage);
            return status;
        }
    }

    sixel_writer_unlock(storage);
    return SIXEL_OK;
}

static SIXELSTATUS
sixel_writer_vtbl_write(sixel_writer_t *self, char const *data, int size)
{
    SIXELSTATUS status;
    sixel_writer_storage_t *storage;

    if (self == NULL || data == NULL || size < 0) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_writer_storage_from_interface(self);
    sixel_writer_lock(storage);
    status = storage->controls.penetrate_multiplexer
        ? sixel_writer_write_penetrated_locked(storage, data, size)
        : sixel_writer_write_raw_locked(storage, data, size);
    sixel_writer_unlock(storage);

    return status;
}

static SIXELSTATUS
sixel_writer_vtbl_end_image(sixel_writer_t *self)
{
    SIXELSTATUS status;
    sixel_writer_storage_t *storage;
    char const *end;
    int end_size;

    if (self == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    storage = sixel_writer_storage_from_interface(self);
    if (storage->controls.skip_dcs_envelope &&
        !storage->controls.penetrate_multiplexer) {
        return SIXEL_OK;
    }

    sixel_writer_lock(storage);
    if (storage->controls.penetrate_multiplexer) {
        if (!storage->used_penetration) {
            sixel_writer_unlock(storage);
            return SIXEL_OK;
        }
        end = DCS_7BIT("\033") DCS_7BIT("\\");
        end_size = (DCS_START_7BIT_SIZE + 1 + DCS_END_7BIT_SIZE) * 2;
    } else if (storage->controls.has_8bit_control) {
        end = DCS_END_8BIT;
        end_size = DCS_END_8BIT_SIZE;
    } else {
        end = DCS_END_7BIT;
        end_size = DCS_END_7BIT_SIZE;
    }
    status = sixel_writer_write_raw_locked(storage, end, end_size);
    storage->used_penetration = 0;
    sixel_writer_unlock(storage);

    return status;
}

SIXEL_INTERNAL_API SIXELSTATUS
sixel_writer_factory_new(sixel_allocator_t *allocator, void **object)
{
    sixel_writer_storage_t *storage;
    SIXELSTATUS status;

    if (allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *object = NULL;
    storage = (sixel_writer_storage_t *)sixel_allocator_malloc(
        allocator,
        sizeof(*storage));
    if (storage == NULL) {
        sixel_helper_set_additional_message(
            "sixel_writer_factory_new: sixel_allocator_malloc() failed.");
        return SIXEL_BAD_ALLOCATION;
    }

    memset(storage, 0, sizeof(*storage));
    storage->interface.vtbl = &g_sixel_writer_vtbl;
    storage->ref = 1U;
    storage->allocator = allocator;
    status = (SIXELSTATUS)sixel_mutex_init(&storage->mutex);
    if (SIXEL_SUCCEEDED(status)) {
        storage->mutex_ready = 1;
    }
    sixel_allocator_ref(allocator);
    *object = &storage->interface;

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
