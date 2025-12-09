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

#include "config.h"

#if HAVE_WIC

#if !defined(_WIN32_WINNT)
# define _WIN32_WINNT 0x0600
#endif

#if HAVE_WINDOWS_H
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

SIXELSTATUS
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
    unsigned char          *pixels;
    ULONG                   decoded_frames;
    PROPVARIANT             lp;
    WICColor                c;
    int                     set_palette;

    set_palette = 0;

    (void) fstatic;
    (void) reqcolors;
    (void) bgcolor;
    (void) loop_control;

    PropVariantInit(&prop);
    PropVariantInit(&lp);

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY);
    if (FAILED(hr)) {
        goto end;
    }

    hr = CoCreateInstance(&CLSID_WICImagingFactory,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory,
                          (LPVOID)&factory);
    if (FAILED(hr)) {
        goto end;
    }

    hr = factory->lpVtbl->CreateStream(factory, &stream);
    if (FAILED(hr)) {
        goto end;
    }

    hr = stream->lpVtbl->InitializeFromMemory(stream,
                                              (WICInProcPointer)pchunk->buffer,
                                              (DWORD)pchunk->size);
    if (FAILED(hr)) {
        goto end;
    }

    hr = factory->lpVtbl->CreateDecoderFromStream(factory,
                                                  (IStream *)stream,
                                                  NULL,
                                                  WICDecodeMetadataCacheOnLoad,
                                                  &decoder);
    if (FAILED(hr)) {
        goto end;
    }

    hr = decoder->lpVtbl->GetFrame(decoder, 0, &wicframe);
    if (FAILED(hr)) {
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

    hr = decoder->lpVtbl->GetFrameCount(decoder, &frame_count);
    if (FAILED(hr)) {
        goto end;
    }

    decoded_frames = 0;
    while (decoded_frames < frame_count) {
        hr = decoder->lpVtbl->GetFrame(decoder, decoded_frames, &wicframe);
        if (FAILED(hr)) {
            goto end;
        }

        if (decoded_frames == 0 && set_palette) {
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
                goto end;
            }

            hr = conv->lpVtbl->Initialize(conv, (IWICBitmapSource*)wicframe,
                                          &GUID_WICPixelFormat32bppRGBA,
                                          WICBitmapDitherTypeNone, NULL, 0.0,
                                          WICBitmapPaletteTypeCustom);
            if (FAILED(hr)) {
                goto end;
            }

            src = (IWICBitmapSource*)conv;
            comp = 4;
            frame->pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
        }

        hr = src->lpVtbl->GetSize(
            src, (UINT *)&frame->width, (UINT *)&frame->height);
        if (FAILED(hr)) {
            goto end;
        }

        /* check size */
        if (frame->width <= 0) {
            sixel_helper_set_additional_message(
                "load_with_wic: an invalid width parameter detected.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (frame->height <= 0) {
            sixel_helper_set_additional_message(
                "load_with_wic: an invalid width parameter detected.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (frame->width > SIXEL_WIDTH_LIMIT) {
            sixel_helper_set_additional_message(
                "load_with_wic: given width parameter is too huge.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }
        if (frame->height > SIXEL_HEIGHT_LIMIT) {
            sixel_helper_set_additional_message(
                "load_with_wic: given height parameter is too huge.");
            status = SIXEL_BAD_INPUT;
            goto end;
        }

        sixel_frame_set_pixels(frame,
                               sixel_allocator_malloc(
                                   pchunk->allocator,
                                   (size_t)(frame->height * frame->width * comp)));
        pixels = sixel_frame_get_pixels(frame);

        {
            WICRect rc = { 0, 0, (INT)frame->width, (INT)frame->height };
            hr = src->lpVtbl->CopyPixels(
                src,
                &rc,                                        /* prc */
                frame->width * comp,                        /* cbStride */
                (UINT)frame->width * frame->height * comp,  /* cbBufferSize */
                pixels);                                    /* pbBuffer */
            if (FAILED(hr)) {
                goto end;
            }
        }

        status = fn_load(frame, context);
        if (SIXEL_FAILED(status)) {
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

int
loader_can_try_wic(sixel_chunk_t const *chunk)
{
    if (chunk == NULL) {
        return 0;
    }
    if (chunk_is_gif(chunk)) {
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

