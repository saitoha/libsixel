/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021-2025 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
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
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF, OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Windows Imaging Component loader split from loader.c. Keeping the COM-heavy
 * headers local avoids pulling Windows-specific dependencies into unrelated
 * build targets.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_WIC

#include <limits.h>
#include <stdlib.h>

#if !defined(_WIN32_WINNT)
# define _WIN32_WINNT 0x0600
#endif

#if HAVE_WINDOWS_H
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
#include <wincodec.h>

#ifndef WICInProcPointer
/*
 * Some MSYS/MinGW header revisions omit WICInProcPointer. Provide a
 * compatible declaration matching the Windows SDK definition so calls to
 * InitializeFromMemory compile on toolchains lacking the typedef.
 */
typedef BYTE *WICInProcPointer;
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

#include <sixel.h>

#include "allocator.h"
#include "chunk.h"
#include "frame.h"
#include "loader-common.h"
#include "loader-wic.h"
#include "compat_stub.h"

typedef struct sixel_loader_wic_component {
    sixel_loader_component_t base;
    sixel_allocator_t *allocator;
    unsigned int ref;
    int fstatic;
    int fuse_palette;
    int reqcolors;
    unsigned char bgcolor[3];
    int has_bgcolor;
    int loop_control;
    int has_start_frame_no;
    int start_frame_no;
    int ico_minsize;
} sixel_loader_wic_component_t;


#ifndef FACILITY_WINCODEC_ERR
# define FACILITY_WINCODEC_ERR 0x898
#endif

/*
 * Classify HRESULT values into libsixel-specific status buckets so callers
 * can distinguish COM bootstrap issues from codec pipeline failures.
 */
static SIXELSTATUS
loader_wic_status_from_hresult(HRESULT hr)
{
    if (FAILED(hr)) {
        if ((unsigned int)HRESULT_FACILITY(hr) == FACILITY_WINCODEC_ERR) {
            return SIXEL_WIC_ERROR;
        }
        return SIXEL_COM_ERROR;
    }

    return SIXEL_OK;
}

static SIXELSTATUS
wic_parse_animation_start_frame_no(int *start_frame_no)
{
    SIXELSTATUS status;
    char const *env_value;
    char *endptr;
    long parsed;

    status = SIXEL_OK;
    env_value = NULL;
    endptr = NULL;
    parsed = 0;

    *start_frame_no = INT_MIN;
    env_value = sixel_compat_getenv("SIXEL_LOADER_ANIMATION_START_FRAME_NO");
    if (env_value == NULL || env_value[0] == '\0') {
        goto end;
    }

    parsed = strtol(env_value, &endptr, 10);
    if (endptr == env_value || *endptr != '\0') {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO must be an integer.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }
    if (parsed < (long)INT_MIN || parsed > (long)INT_MAX) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is out of range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *start_frame_no = (int)parsed;

end:
    return status;
}

static SIXELSTATUS
wic_resolve_animation_start_frame_no(int start_frame_no,
                                     int frame_count,
                                     int *resolved)
{
    SIXELSTATUS status;
    int index;

    status = SIXEL_OK;
    index = 0;

    if (frame_count <= 0) {
        sixel_helper_set_additional_message(
            "Animation frame count must be positive.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    if (start_frame_no >= 0) {
        index = start_frame_no;
    } else {
        index = frame_count + start_frame_no;
    }

    if (index < 0 || index >= frame_count) {
        sixel_helper_set_additional_message(
            "SIXEL_LOADER_ANIMATION_START_FRAME_NO is outside"
            " the animation frame range.");
        status = SIXEL_BAD_INPUT;
        goto end;
    }

    *resolved = index;

end:
    return status;
}

static SIXELSTATUS
load_with_wic(
    sixel_chunk_t const       /* in */     *pchunk,      /* image data */
    int                       /* in */     fstatic,      /* static */
    int                       /* in */     fuse_palette, /* whether to use */
                                                  /* palette if possible */
    int                       /* in */     reqcolors,    /* reqcolors */
    unsigned char             /* in */     *bgcolor,     /* background */
                                                  /* color */
    int                       /* in */     loop_control, /* one of enum */
                                                  /* loop_control */
    int                       /* in */     start_frame_no_set,
    int                       /* in */     start_frame_no_override,
    int                       /* in */     ico_minsize_override,
    sixel_load_image_function /* in */     fn_load,      /* callback */
    void                      /* in/out */ *context      /* private */
                                                  /* data for callback */
)
{
    HRESULT                 hr         = E_FAIL;
    SIXELSTATUS             status     = SIXEL_FALSE;
    IWICImagingFactory     *factory    = NULL;
    IWICStream             *stream     = NULL;
    IWICBitmapDecoder      *decoder    = NULL;
    IWICBitmapFrameDecode  *wicframe   = NULL;
    IWICFormatConverter    *conv       = NULL;
    IWICBitmapSource       *src        = NULL;
    IWICPalette            *wicpalette = NULL;
    WICColor               *wiccolors  = NULL;
    IWICMetadataQueryReader *qdecoder  = NULL;
    IWICMetadataQueryReader *qframe    = NULL;
    UINT                    ncolors    = 0;
    sixel_frame_t          *frame      = NULL;
    int                     comp       = 4;
    UINT                    actual     = 0;
    BYTE                    format[16] = { 0 };
    PROPVARIANT             prop;
    UINT                    frame_count;
    UINT                    i;
    UINT                    selected_frame_index;
    UINT                    candidate_width;
    UINT                    candidate_height;
    UINT                    candidate_metric;
    UINT                    selected_metric;
    UINT                    fallback_frame_index;
    UINT                    fallback_metric;
    IWICBitmapFrameDecode  *candidate_frame;
    GUID                    container_format;
    int                     is_ico_container;
    int                     ico_minsize;
    unsigned char          *pixels;
    ULONG                   decoded_frames;
    ULONG                   decoded_frames_end;
    PROPVARIANT             lp;
    WICColor                c;
    int                     set_palette;
    int                     start_frame_no;
    int                     resolved_start_frame_no;
    int                     frame_count_int;

    set_palette = 0;
    selected_frame_index = 0;
    candidate_width = 0;
    candidate_height = 0;
    candidate_metric = 0;
    selected_metric = 0;
    fallback_frame_index = 0;
    fallback_metric = 0;
    candidate_frame = NULL;
    memset(&container_format, 0, sizeof(container_format));
    is_ico_container = 0;
    ico_minsize = ico_minsize_override;
    decoded_frames_end = 0;
    start_frame_no = INT_MIN;
    resolved_start_frame_no = INT_MIN;
    frame_count_int = 0;

    (void) fstatic;
    (void) reqcolors;
    (void) bgcolor;
    (void) loop_control;

    PropVariantInit(&prop);
    PropVariantInit(&lp);

    if (start_frame_no_set) {
        start_frame_no = start_frame_no_override;
    } else {
        status = wic_parse_animation_start_frame_no(&start_frame_no);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    hr = CoInitializeEx(NULL,
                        COINIT_APARTMENTTHREADED |
                        COINIT_SPEED_OVER_MEMORY);
    if (FAILED(hr)) {
        sixel_helper_set_additional_message(
            "load_with_wic: CoInitializeEx() failed.");
        goto end;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory,
                          (LPVOID)&factory);
    if (FAILED(hr)) {
        sixel_helper_set_additional_message(
            "load_with_wic: CoCreateInstance(IWICImagingFactory) failed.");
        goto end;
    }

    hr = factory->lpVtbl->CreateStream(factory, &stream);
    if (FAILED(hr)) {
        sixel_helper_set_additional_message(
            "load_with_wic: IWICImagingFactory::CreateStream() failed.");
        goto end;
    }

    hr = stream->lpVtbl->InitializeFromMemory(stream,
                                              (WICInProcPointer)pchunk->buffer,
                                              (DWORD)pchunk->size);
    if (FAILED(hr)) {
        sixel_helper_set_additional_message(
            "load_with_wic: IWICStream::InitializeFromMemory() failed.");
        goto end;
    }

    hr = factory->lpVtbl->CreateDecoderFromStream(factory,
                                                  (IStream *)stream,
                                                  NULL,
                                                  WICDecodeMetadataCacheOnLoad,
                                                  &decoder);
    if (FAILED(hr)) {
        sixel_helper_set_additional_message(
            "load_with_wic: IWICImagingFactory::"
            "CreateDecoderFromStream() failed.");
        goto end;
    }

    hr = decoder->lpVtbl->GetFrameCount(decoder, &frame_count);
    if (FAILED(hr)) {
        sixel_helper_set_additional_message(
            "load_with_wic: IWICBitmapDecoder::GetFrameCount() failed.");
        goto end;
    }
    if (frame_count == 0) {
        sixel_helper_set_additional_message(
            "load_with_wic: decoder reported zero frames.");
        status = SIXEL_WIC_ERROR;
        hr = E_FAIL;
        goto end;
    }

    frame_count_int = (int)frame_count;
    if (start_frame_no != INT_MIN) {
        status = wic_resolve_animation_start_frame_no(
            start_frame_no,
            frame_count_int,
            &resolved_start_frame_no);
        if (SIXEL_FAILED(status)) {
            hr = E_FAIL;
            goto end;
        }
    }

    hr = decoder->lpVtbl->GetContainerFormat(decoder, &container_format);
    if (SUCCEEDED(hr)) {
        if (IsEqualGUID(&container_format, &GUID_ContainerFormatIco)) {
            is_ico_container = 1;
        }
    }

    if (is_ico_container && ico_minsize > 0) {
        for (i = 0; i < frame_count; ++i) {
            hr = decoder->lpVtbl->GetFrame(decoder, i, &candidate_frame);
            if (FAILED(hr)) {
                sixel_helper_set_additional_message(
                    "load_with_wic: IWICBitmapDecoder::GetFrame() failed "
                    "during ICO frame selection.");
                goto end;
            }

            hr = candidate_frame->lpVtbl->GetSize(candidate_frame,
                                                  &candidate_width,
                                                  &candidate_height);
            if (FAILED(hr)) {
                sixel_helper_set_additional_message(
                    "load_with_wic: IWICBitmapFrameDecode::GetSize() failed "
                    "during ICO frame selection.");
                goto end;
            }

            if (candidate_width > candidate_height) {
                candidate_metric = candidate_width;
            } else {
                candidate_metric = candidate_height;
            }

            if (candidate_metric >= (UINT)ico_minsize) {
                if (selected_metric == 0 ||
                    candidate_metric < selected_metric) {
                    selected_metric = candidate_metric;
                    selected_frame_index = i;
                }
            } else if (candidate_metric > fallback_metric) {
                fallback_metric = candidate_metric;
                fallback_frame_index = i;
            }

            candidate_frame->lpVtbl->Release(candidate_frame);
            candidate_frame = NULL;
        }

        if (selected_metric == 0 && fallback_metric > 0) {
            selected_frame_index = fallback_frame_index;
        }
    }

    if (!is_ico_container && resolved_start_frame_no != INT_MIN) {
        /*
         * WIC currently decodes one selected frame per invocation in this
         * pipeline.  Apply start-frame selection only to the first pass.
         */
        selected_frame_index = (UINT)resolved_start_frame_no;
    }

    hr = decoder->lpVtbl->GetFrame(decoder,
                                   selected_frame_index,
                                   &wicframe);
    if (FAILED(hr)) {
        sixel_helper_set_additional_message(
            "load_with_wic: IWICBitmapDecoder::GetFrame() failed.");
        goto end;
    }
    hr = wicframe->lpVtbl->GetMetadataQueryReader(wicframe, &qframe);
    if (SUCCEEDED(hr)) {
        hr = wicframe->lpVtbl->CopyPalette(wicframe, NULL);
        if (FAILED(hr)) {
            set_palette = 0;
        } else {
            set_palette = fuse_palette;
        }
        hr = qframe->lpVtbl->GetContainerFormat(qframe, (GUID *)&format);
        if (FAILED(hr)) {
            sixel_helper_set_additional_message(
                "load_with_wic: IWICMetadataQueryReader::GetContainerFormat() "
                "failed.");
            goto end;
        }

        hr = qframe->lpVtbl->GetMetadataByName(qframe,
                                               L"/ifd/exif:Flash",
                                               &prop);
        if (SUCCEEDED(hr)) {
            if (prop.vt == VT_I2) {
                if (prop.iVal & 0x0040) {
                    set_palette = 0;
                }
            }
        }

        if (set_palette) {
            hr = decoder->lpVtbl->GetMetadataQueryReader(decoder, &qdecoder);
            if (SUCCEEDED(hr)) {
                hr = qdecoder->lpVtbl->GetMetadataByName(qdecoder,
                                                         L"/log2Chan",
                                                         &lp);
                if (SUCCEEDED(hr)) {
                    if (lp.vt == VT_I2) {
                        comp = (lp.iVal + 7) / 8;
                    }
                }
            }
        }
    }

    status = sixel_frame_new(&frame, pchunk->allocator);
    if (SIXEL_FAILED(status)) {
        sixel_helper_set_additional_message(
            "load_with_wic: sixel_frame_new() failed.");
        hr = E_FAIL;
        goto end;
    }

    decoded_frames = selected_frame_index;
    /* ICO is not animation content for this pipeline. */
    /* Decode only the selected frame to avoid size mismatch paths. */
    decoded_frames_end = (ULONG)frame_count;
    if (is_ico_container) {
        decoded_frames_end = decoded_frames + 1u;
    }

    while (decoded_frames < decoded_frames_end) {
        hr = decoder->lpVtbl->GetFrame(decoder, decoded_frames, &wicframe);
        if (FAILED(hr)) {
            sixel_helper_set_additional_message(
                "load_with_wic: IWICBitmapDecoder::GetFrame() failed while "
                "decoding.");
            goto end;
        }

        if (decoded_frames == selected_frame_index && set_palette) {
            hr = factory->lpVtbl->CreatePalette(factory, &wicpalette);
            if (SUCCEEDED(hr)) {
                hr = wicpalette->lpVtbl->InitializeFromBitmap(
                    wicpalette, (IWICBitmapSource*)wicframe, 256, FALSE);
                if (SUCCEEDED(hr)) {
                    hr = wicpalette->lpVtbl->GetColorCount(
                        wicpalette, &ncolors);
                    if (SUCCEEDED(hr) && ncolors > 0) {
                        wiccolors = (WICColor *)sixel_allocator_malloc(
                            pchunk->allocator,
                            (size_t)ncolors * sizeof(WICColor));
                        if (wiccolors == NULL) {
                            sixel_helper_set_additional_message(
                                "load_with_wic: malloc failed.");
                            status = SIXEL_BAD_ALLOCATION;
                            hr = E_FAIL;
                            goto end;
                        }
                        frame->palette = (unsigned char *)
                            sixel_allocator_malloc(
                                pchunk->allocator,
                                (size_t)ncolors * 3);
                        if (frame->palette == NULL) {
                            sixel_helper_set_additional_message(
                                "load_with_wic: malloc failed.");
                            status = SIXEL_BAD_ALLOCATION;
                            hr = E_FAIL;
                            goto end;
                        }
                        hr = wicpalette->lpVtbl->GetColors(
                            wicpalette, ncolors, wiccolors, &actual);
                        if (SUCCEEDED(hr) && actual == ncolors) {
                            for (i = 0; i < ncolors; ++i) {
                                c = wiccolors[i];
                                frame->palette[i * 3 + 0] =
                                    (unsigned char)((c >> 16) & 0xFF);
                                frame->palette[i * 3 + 1] =
                                    (unsigned char)((c >> 8) & 0xFF);
                                frame->palette[i * 3 + 2] =
                                    (unsigned char)(c & 0xFF);
                            }
                            frame->ncolors = (int)ncolors;
                        } else {
                            hr = E_FAIL;
                        }
                    }
                }
            }
            if (FAILED(hr)) {
                if (conv) {
                    conv->lpVtbl->Release(conv);
                    conv = NULL;
                }
                sixel_allocator_free(pchunk->allocator, frame->palette);
                frame->palette = NULL;
                sixel_allocator_free(pchunk->allocator, wiccolors);
                wiccolors = NULL;
                src = NULL;
            }
        }

        if (src == NULL) {
            hr = factory->lpVtbl->CreateFormatConverter(factory, &conv);
            if (FAILED(hr)) {
                sixel_helper_set_additional_message(
                    "load_with_wic: IWICImagingFactory::"
                    "CreateFormatConverter() failed.");
                goto end;
            }

            hr = conv->lpVtbl->Initialize(conv, (IWICBitmapSource*)wicframe,
                                          &GUID_WICPixelFormat32bppRGBA,
                                          WICBitmapDitherTypeNone, NULL, 0.0,
                                          WICBitmapPaletteTypeCustom);
            if (FAILED(hr)) {
                sixel_helper_set_additional_message(
                    "load_with_wic: IWICFormatConverter::Initialize() failed.");
                goto end;
            }

            src = (IWICBitmapSource*)conv;
            comp = 4;
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
            frame->colorspace = SIXEL_COLORSPACE_GAMMA;
        }

        hr = src->lpVtbl->GetSize(
            src, (UINT *)&frame->width, (UINT *)&frame->height);
        if (FAILED(hr)) {
            sixel_helper_set_additional_message(
                "load_with_wic: IWICBitmapSource::GetSize() failed.");
            goto end;
        }

        /* check size */
        if (frame->width <= 0) {
            sixel_helper_set_additional_message(
                "load_with_wic: an invalid width parameter detected.");
            status = SIXEL_BAD_INPUT;
            hr = E_FAIL;
            goto end;
        }
        if (frame->height <= 0) {
            sixel_helper_set_additional_message(
                "load_with_wic: an invalid height parameter detected.");
            status = SIXEL_BAD_INPUT;
            hr = E_FAIL;
            goto end;
        }
        if (frame->width > SIXEL_WIDTH_LIMIT) {
            sixel_helper_set_additional_message(
                "load_with_wic: given width parameter is too huge.");
            status = SIXEL_BAD_INPUT;
            hr = E_FAIL;
            goto end;
        }
        if (frame->height > SIXEL_HEIGHT_LIMIT) {
            sixel_helper_set_additional_message(
                "load_with_wic: given height parameter is too huge.");
            status = SIXEL_BAD_INPUT;
            hr = E_FAIL;
            goto end;
        }

        sixel_frame_set_pixels(frame,
                               sixel_allocator_malloc(
                                   pchunk->allocator,
                                   (size_t)(frame->height *
                                            frame->width * comp)));
        pixels = sixel_frame_get_pixels(frame);
        if (pixels == NULL) {
            sixel_helper_set_additional_message(
                "load_with_wic: sixel_allocator_malloc() failed.");
            status = SIXEL_BAD_ALLOCATION;
            hr = E_FAIL;
            goto end;
        }

        {
            WICRect rc = { 0, 0, (INT)frame->width, (INT)frame->height };
            hr = src->lpVtbl->CopyPixels(
                src,
                &rc,                                        /* prc */
                frame->width * comp,                        /* cbStride */
                (UINT)frame->width * frame->height * comp,  /* cbBufferSize */
                pixels);                                    /* pbBuffer */
            if (FAILED(hr)) {
                sixel_helper_set_additional_message(
                    "load_with_wic: IWICBitmapSource::CopyPixels() failed.");
                goto end;
            }
        }

        status = fn_load(frame, context);
        if (SIXEL_FAILED(status)) {
            sixel_helper_set_additional_message(
                "load_with_wic: frame callback returned failure.");
            hr = E_FAIL;
            goto end;
        }

        if (conv) {
             conv->lpVtbl->Release(conv);
        }
        if (wicpalette) {
             wicpalette->lpVtbl->Release(wicpalette);
        }
        if (wiccolors) {
             sixel_allocator_free(pchunk->allocator, wiccolors);
        }
        if (wicframe) {
             wicframe->lpVtbl->Release(wicframe);
        }
        if (qdecoder) {
             qdecoder->lpVtbl->Release(qdecoder);
        }
        if (qframe) {
             qframe->lpVtbl->Release(qframe);
        }
        if (stream) {
             stream->lpVtbl->Release(stream);
        }
        if (factory) {
             factory->lpVtbl->Release(factory);
        }

        sixel_frame_unref(frame);

        CoUninitialize();

        if (FAILED(hr)) {
            return loader_wic_status_from_hresult(hr);
        }

        return SIXEL_OK;
    }

end:
    if (conv) {
         conv->lpVtbl->Release(conv);
    }
    if (wicpalette) {
         wicpalette->lpVtbl->Release(wicpalette);
    }
    if (wiccolors) {
         sixel_allocator_free(pchunk->allocator, wiccolors);
    }
    if (candidate_frame) {
         candidate_frame->lpVtbl->Release(candidate_frame);
    }
    if (wicframe) {
         wicframe->lpVtbl->Release(wicframe);
    }
    if (qdecoder) {
         qdecoder->lpVtbl->Release(qdecoder);
    }
    if (qframe) {
         qframe->lpVtbl->Release(qframe);
    }
    if (stream) {
         stream->lpVtbl->Release(stream);
    }
    if (factory) {
         factory->lpVtbl->Release(factory);
    }
    sixel_frame_unref(frame);

    CoUninitialize();

    if (FAILED(hr)) {
        if (sixel_helper_get_additional_message() == NULL) {
            sixel_helper_set_additional_message(
                "load_with_wic: unexpected WIC backend failure.");
        }
        return loader_wic_status_from_hresult(hr);
    }

    return SIXEL_OK;
}


static void
sixel_loader_wic_ref(sixel_loader_component_t *component)
{
    sixel_loader_wic_component_t *self;

    self = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_wic_component_t *)component;
    ++self->ref;
}

static void
sixel_loader_wic_unref(sixel_loader_component_t *component)
{
    sixel_loader_wic_component_t *self;
    sixel_allocator_t *allocator;

    self = NULL;
    allocator = NULL;
    if (component == NULL) {
        return;
    }

    self = (sixel_loader_wic_component_t *)component;
    if (self->ref == 0u) {
        return;
    }

    --self->ref;
    if (self->ref > 0u) {
        return;
    }

    allocator = self->allocator;
    sixel_allocator_free(allocator, self);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
sixel_loader_wic_setopt(sixel_loader_component_t *component,
                        int option,
                        void const *value)
{
    sixel_loader_wic_component_t *self;
    int const *flag;
    unsigned char const *color;

    self = NULL;
    flag = NULL;
    color = NULL;
    if (component == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_wic_component_t *)component;
    switch (option) {
    case SIXEL_LOADER_OPTION_REQUIRE_STATIC:
        flag = (int const *)value;
        self->fstatic = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_USE_PALETTE:
        flag = (int const *)value;
        self->fuse_palette = flag != NULL ? *flag : 0;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_REQCOLORS:
        flag = (int const *)value;
        if (flag != NULL) {
            self->reqcolors = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_BGCOLOR:
        if (value == NULL) {
            self->has_bgcolor = 0;
            return SIXEL_OK;
        }
        color = (unsigned char const *)value;
        self->bgcolor[0] = color[0];
        self->bgcolor[1] = color[1];
        self->bgcolor[2] = color[2];
        self->has_bgcolor = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_LOOP_CONTROL:
        flag = (int const *)value;
        if (flag != NULL) {
            self->loop_control = *flag;
        }
        return SIXEL_OK;
    case SIXEL_LOADER_OPTION_START_FRAME_NO:
        if (value == NULL) {
            self->has_start_frame_no = 0;
            self->start_frame_no = INT_MIN;
            return SIXEL_OK;
        }
        flag = (int const *)value;
        self->start_frame_no = *flag;
        self->has_start_frame_no = 1;
        return SIXEL_OK;
    case SIXEL_LOADER_COMPONENT_OPTION_WIC_ICO_MINSIZE:
        flag = (int const *)value;
        self->ico_minsize = (flag != NULL && *flag > 0) ? *flag : 0;
        return SIXEL_OK;
    default:
        return SIXEL_OK;
    }
}

static SIXELSTATUS
sixel_loader_wic_load(sixel_loader_component_t *component,
                      sixel_chunk_t const *chunk,
                      sixel_load_image_function fn_load,
                      void *context)
{
    sixel_loader_wic_component_t *self;
    unsigned char *bgcolor;

    self = NULL;
    bgcolor = NULL;
    if (component == NULL || chunk == NULL || fn_load == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    self = (sixel_loader_wic_component_t *)component;
    if (self->has_bgcolor) {
        bgcolor = self->bgcolor;
    }

    return load_with_wic(chunk,
                         self->fstatic,
                         self->fuse_palette,
                         self->reqcolors,
                         bgcolor,
                         self->loop_control,
                         self->has_start_frame_no,
                         self->start_frame_no,
                         self->ico_minsize,
                         fn_load,
                         context);
}

static char const *
sixel_loader_wic_name(sixel_loader_component_t const *component)
{
    (void)component;
    return "wic";
}

static sixel_loader_component_vtbl_t const g_sixel_loader_wic_vtbl = {
    sixel_loader_wic_ref,
    sixel_loader_wic_unref,
    sixel_loader_wic_setopt,
    sixel_loader_wic_load,
    sixel_loader_wic_name
};

SIXELSTATUS
sixel_loader_wic_new(sixel_allocator_t *allocator,
                     sixel_loader_component_t **ppcomponent)
{
    sixel_loader_wic_component_t *self;

    self = NULL;
    if (allocator == NULL || ppcomponent == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *ppcomponent = NULL;
    self = (sixel_loader_wic_component_t *)
        sixel_allocator_malloc(allocator, sizeof(*self));
    if (self == NULL) {
        return SIXEL_BAD_ALLOCATION;
    }

    memset(self, 0, sizeof(*self));
    self->base.vtbl = &g_sixel_loader_wic_vtbl;
    self->allocator = allocator;
    self->ref = 1u;
    self->reqcolors = SIXEL_PALETTE_MAX;
    self->loop_control = SIXEL_LOOP_AUTO;
    self->start_frame_no = INT_MIN;
    self->ico_minsize = 0;
    sixel_allocator_ref(allocator);
    *ppcomponent = &self->base;
    return SIXEL_OK;
}

int
loader_can_try_wic(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }

    return 1;
}

#endif /* HAVE_WIC */

#if !HAVE_WIC
/*
 * Provide a placeholder to keep the unit non-empty when WIC is disabled.
 */
typedef int loader_wic_disabled;
#endif

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
