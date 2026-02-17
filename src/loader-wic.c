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

SIXELAPI SIXELSTATUS
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
    ico_minsize = 0;
    decoded_frames_end = 0;

    (void) fstatic;
    (void) reqcolors;
    (void) bgcolor;
    (void) loop_control;

    PropVariantInit(&prop);
    PropVariantInit(&lp);

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY);
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

    hr = decoder->lpVtbl->GetContainerFormat(decoder, &container_format);
    if (SUCCEEDED(hr)) {
        if (IsEqualGUID(&container_format, &GUID_ContainerFormatIco)) {
            is_ico_container = 1;
        }
    }

    ico_minsize = loader_wic_get_ico_minsize();
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
                                   (size_t)(frame->height * frame->width * comp)));
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
            return SIXEL_FALSE;
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
        return SIXEL_FALSE;
    }

    return SIXEL_OK;
}

SIXELAPI int
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
