/*
 * Copyright (c) 2025 libsixel developers. See `AUTHORS`.
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
 */
#include "config.h"

#include "wic_stub.h"
#include <strsafe.h>

#include <sixel.h>

#define WIC_DISABLE_THUMBNAIL_CACHE 0
#define WIC_PAL8_AS_DEFAULT_PIXELFORMAT 0

#ifndef EXIF_COLORSPACE_SRGB
# define EXIF_COLORSPACE_SRGB       1
#endif
# ifndef EXIF_COLORSPACE_UNCALIBRATED
# define EXIF_COLORSPACE_UNCALIBRATED 0xFFFF
#endif

extern IMAGE_DOS_HEADER __ImageBase;

/* CLSID */
static const CLSID CLSID_SixelDecoder = {
    /* 15b9b4da-b155-4977-8571-cf005884bcb9 */
    0x15b9b4da,
    0xb155,
    0x4977,
    { 0x85, 0x71, 0xcf, 0x00, 0x58, 0x84, 0xbc, 0xb9 }
};

/* ContainerFormat */
static const GUID GUID_ContainerFormatSIXEL = {
    /* 5b2053a9-7a2e-4e0e-9d4c-96532364401a */
    0x5b2053a9,
    0x7a2e,
    0x4e0e,
    { 0x9d, 0x4c, 0x96, 0x53, 0x23, 0x64, 0x40, 0x1a }
};

#define CLSIDSTR_SixelDecoder \
    L"{15B9B4DA-B155-4977-8571-CF005884BCB9}"
#define GUIDSTR_ContainerFormatSIXEL \
    L"{5B2053A9-7A2E-4E0E-9D4C-96532364401A}"
#define GUIDSTR_VendorSIXEL \
    L"{0B0A6D1E-0C4F-42E9-BA37-1FF5636E9A55}"
#define CATIDSTR_WICBitmapDecoders \
    L"{7ED96837-96F0-4812-B211-F13C24117ED3}"

/* GUID_WICPixelFormat32bppBGRA */
#define GUIDSTR_WICPixelFormat32bppBGRA \
    L"{6FDDC324-4E03-4BFE-B185-3D77768DC90F}"
#define GUIDSTR_WICPixelFormat8bppIndexed \
    L"{6FDDC324-4E03-4BFE-B185-3D77768DC904}"

/* Windows Photo Viewer */
#define CLSIDSTR_WindowsPhotoViewer \
    L"{FFE2A43C-56B9-4bf5-9A79-CC6D4285608A}"

/* Thumbnail Handler */
#define IIDSTR_IThumbnailProvider \
    L"{E357FCCD-A995-4576-B01F-234630154E96}"

#define CLSIDSTR_PhotoThumbnailProvider \
    L"{C7657C4A-9F68-40fa-A4DF-96BC08EB3551}"

/* Frame object
   implements: IWICBitmapFrameDecode
               IWICBitmapSourceTransform
*/
typedef struct {
    /* frame vtbl pointer */
    const IWICBitmapFrameDecodeVtbl      *lpVtbl;
    /* transform vtbl pointer */
    const IWICBitmapSourceTransformVtbl  *lpTransVtbl;
    /* ref counter */
    LONG ref;
    /* width */
    UINT w;
    /* height */
    UINT h;
    /* 8bpp indexed pixels */
    BYTE *indices;
    /* ARGB palette */
    WICColor *palette;
    /* num of colors */
    UINT ncolors;
} SixelFrame;

/* IUnknown */
static HRESULT STDMETHODCALLTYPE
SixelFrame_QueryInterface(
    IWICBitmapFrameDecode              *iface,
    REFIID                /* [in] */    riid,
    void                  /* [out] */ **ppv
)
{
    SixelFrame *f;

    if (! ppv) {
        return E_POINTER;
    }
    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    } else if (IsEqualIID(riid, &IID_IWICBitmapSource)) {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    } else if (IsEqualIID(riid, &IID_IWICBitmapFrameDecode)) {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    } else if (IsEqualIID(riid, &IID_IWICBitmapSourceTransform)) {
        f = (SixelFrame*)iface;
        *ppv = (void*)&f->lpTransVtbl;
        IUnknown_AddRef(iface);
        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
SixelFrame_AddRef(
    IWICBitmapFrameDecode *iface
)
{
    SixelFrame *f;

    f = (SixelFrame*)iface;

    return (ULONG)InterlockedIncrement(&f->ref);
}

static ULONG STDMETHODCALLTYPE
SixelFrame_Release(
    IWICBitmapFrameDecode *iface
)
{
    SixelFrame *f;
    ULONG n;

    f = (SixelFrame*)iface;

    n = (ULONG)InterlockedDecrement(&f->ref);

    if (n == 0) {
        if (f->indices) {
            CoTaskMemFree(f->indices);
        }
        if (f->palette) {
            CoTaskMemFree(f->palette);
        }
        CoTaskMemFree(f);
    }

    return n;
}

/* IIWICBitmapSource */

/* IIWICBitmapSource::GetSize
 *
 * wincodec.h
 *
 * HRESULT IWICBitmapSource::GetSize(
 *   [out] UINT *puiWidth,
 *   [out] UINT *puiHeight
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_GetSize(
    IWICBitmapFrameDecode             *iface,
    UINT                  /* [out] */ *puiWidth,
    UINT                  /* [out] */ *puiHeight
)
{
    SixelFrame *f;

    (void) iface;

    f = (SixelFrame*)iface;

    if(puiWidth == NULL || puiHeight == NULL) {
        return E_INVALIDARG;  /* or E_POINTER? */
    }

    *puiWidth = f->w;
    *puiHeight = f->h;

    return S_OK;
}

/* IIWICBitmapSource::GetPixelFormat
 *
 * wincodec.h
 *
 * HRESULT IWICBitmapSource::GetPixelFormat(
 *   [out] WICPixelFormatGUID *pPixelFormat,
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_GetPixelFormat(
    IWICBitmapFrameDecode             *iface,
    WICPixelFormatGUID    /* [out] */ *pPixelFormat
)
{
    (void) iface;

    if (pPixelFormat == NULL) {
        return E_INVALIDARG;
    }

    /* default to BGRA */
#if WIC_PAL8_AS_DEFAULT_PIXELFORMAT
    *pPixelFormat = GUID_WICPixelFormat8bppIndexed;
#else
    *pPixelFormat = GUID_WICPixelFormat32bppBGRA;
#endif

    return S_OK;
}

/* IIWICBitmapSource::GetResolution
 *
 * wincodec.h
 *
 * HRESULT GetResolution(
 *   [out] double *pDpiX,
 *   [out] double *pDpiY
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_GetResolution(
    IWICBitmapFrameDecode             *iface,
    double                /* [out] */ *pDpiX,
    double                /* [out] */ *pDpiY
)
{
    (void) iface;

    if (pDpiX == NULL || pDpiY == NULL) {
        return E_INVALIDARG;
    }

    *pDpiX = 96.0;
    *pDpiY = 96.0;

    return S_OK;
}

/* IIWICBitmapSource::CopyPalette
 *
 * wincodec.h
 *
 * HRESULT CopyPalette(
 *   [in] IWICPalette *pIPalette
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_CopyPalette(
    IWICBitmapFrameDecode            *iface,
    IWICPalette           /* [in] */ *pIPalette
)
{
    SixelFrame *f;

    if (pIPalette == NULL) {
        return E_INVALIDARG;
    }

    f = (SixelFrame*)iface;

    return IWICPalette_InitializeCustom(pIPalette, f->palette, f->ncolors);
}

/* IIWICBitmapSource::CopyPixels
 *
 * wincodec.h
 *
 * HRESULT CopyPixels(
 *   [in]  const WICRect *prc,
 *   [in]  UINT          cbStride,
 *   [in]  UINT          cbBufferSize,
 *   [out] BYTE          *pbBuffer
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_CopyPixels(
    IWICBitmapFrameDecode             *iface,
    const WICRect*        /* [in]  */  prc,
    UINT                  /* [in]  */  cbStride,
    UINT                  /* [in]  */  cbBufferSize,
    BYTE                  /* [out] */ *dst
)
{
    SixelFrame *f;
    UINT x = 0;
    UINT y = 0;
    UINT rw;
    UINT rh;
    BYTE *src;
    UINT i;
#if ! WIC_PAL8_AS_DEFAULT_PIXELFORMAT
    UINT j;
    BYTE idx;
    WICColor c;
    BYTE *d;
#endif

    f = (SixelFrame*)iface;
    if (dst == NULL) {
        return E_INVALIDARG;
    }
    rw = f->w;
    rh = f->h;
    if (prc != NULL) {
        x  = prc->X;
        y  = prc->Y;
        rw = prc->Width;
        rh = prc->Height;
    }

#if WIC_PAL8_AS_DEFAULT_PIXELFORMAT
    if(cbStride == 0) {
        cbStride = rw;
    }

    if(cbBufferSize < cbStride * rh) {
        return E_INVALIDARG;
    }

    src = f->indices + y * f->w + x;
    for(i = 0; i < rh; ++i) {
        memcpy(dst + i * cbStride,
               src + i * f->w,
               rw);
    }
#else
    if (cbStride == 0) {
        cbStride = rw * 4;
    }

    if (cbBufferSize < cbStride * rh) {
        return E_INVALIDARG;
    }

    src = f->indices + y * f->w + x;
    for (i = 0; i < rh; ++i) {
        for (j = 0; j < rw; ++j) {
            idx = src[i * f->w + j];
            c = f->palette[idx];
            d = dst + i * cbStride + j * 4;
            d[0] = (BYTE)(c & 0xFF);         /* B */
            d[1] = (BYTE)((c >> 8) & 0xFF);  /* G */
            d[2] = (BYTE)((c >> 16) & 0xFF); /* R */
            d[3] = (BYTE)((c >> 24) & 0xFF); /* A */
        }
    }
#endif

    return S_OK;
}

/* IWICBitmapSourceTransform (Aggregated) */

/* IUnknown (IWICBitmapSourceTransform) */
static HRESULT STDMETHODCALLTYPE
SixelFrame_Transform_QueryInterface(
    IWICBitmapSourceTransform              *iface,
    REFIID                    /* [in] */    riid,
    void                      /* [out] */ **ppv)
{
    SixelFrame *f;
    HRESULT hr = E_FAIL;

    f = (SixelFrame*)((BYTE*)iface - offsetof(SixelFrame, lpTransVtbl));

    hr = IUnknown_QueryInterface((IWICBitmapFrameDecode*)f, riid, ppv);
    if (FAILED(hr)) {
        return hr;
    }

    return S_OK;
}

static ULONG STDMETHODCALLTYPE
SixelFrame_Transform_AddRef(
    IWICBitmapSourceTransform *iface
)
{
    SixelFrame *f;

    f = (SixelFrame*)((BYTE*)iface - offsetof(SixelFrame, lpTransVtbl));

    return SixelFrame_AddRef((IWICBitmapFrameDecode*)f);
}

static ULONG STDMETHODCALLTYPE
SixelFrame_Transform_Release(
    IWICBitmapSourceTransform *iface
)
{
    SixelFrame *f;

    f = (SixelFrame*)((BYTE*)iface - offsetof(SixelFrame, lpTransVtbl));

    return SixelFrame_Release((IWICBitmapFrameDecode*)f);
}

/* IWICBitmapSourceTransform::CopyPixels
 *
 * wincodec.h
 *
 * HRESULT CopyPixels(
 *   [in]  const WICRect             *prc,
 *   [in]  UINT                      uiWidth,
 *   [in]  UINT                      uiHeight,
 *   [in]  WICPixelFormatGUID        *pguidDstFormat,
 *   [in]  WICBitmapTransformOptions dstTransform,
 *   [in]  UINT                      nStride,
 *   [in]  UINT                      cbBufferSize,
 *   [out] BYTE                      *pbBuffer
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_Transform_CopyPixels(
    IWICBitmapSourceTransform *iface,
    const WICRect              /* [in] */  *prc,
    UINT                       /* [in] */   uiWidth,
    UINT                       /* [in] */   uiHeight,
    WICPixelFormatGUID         /* [in] */  *pguidDstFormat,
    WICBitmapTransformOptions  /* [in] */   dstTransform,
    UINT                       /* [in] */   nStride,
    UINT                       /* [in] */   cbBufferSize,
    BYTE                       /* [out] */ *pbBuffer)
{
    SixelFrame *f;
    UINT x = 0;
    UINT y = 0;
    UINT rw;
    UINT rh;
    UINT i;
    UINT j;
    BYTE *src;
    BYTE idx;
    WICColor c;
    BYTE *d;

    f = (SixelFrame*)((BYTE*)iface - offsetof(SixelFrame, lpTransVtbl));

    if (dstTransform != WICBitmapTransformRotate0) {
        return WINCODEC_ERR_UNSUPPORTEDOPERATION;
    }
    if (uiWidth != f->w || uiHeight != f->h) {
        return WINCODEC_ERR_UNSUPPORTEDOPERATION;
    }
    if (prc) {
        x = prc->X;
        y = prc->Y;
        rw = prc->Width;
        rh = prc->Height;
    } else {
        rw = f->w;
        rh = f->h;
    }

    if (IsEqualGUID(pguidDstFormat, &GUID_WICPixelFormat32bppBGRA)) {
        if (nStride == 0) {
            nStride = rw * 4;
        }
        if (cbBufferSize < nStride * rh) {
            return E_INVALIDARG;
        }
        src = f->indices + y * f->w + x;
        for (i = 0; i < rh; ++i) {
            for (j = 0; j < rw; ++j) {
                idx = src[i * f->w + j];
                c = f->palette[idx];
                d = pbBuffer + i * nStride + j * 4;
                d[0] = (BYTE)(c & 0xFF);         /* B */
                d[1] = (BYTE)((c >> 8) & 0xFF);  /* G */
                d[2] = (BYTE)((c >> 16) & 0xFF); /* R */
                d[3] = (BYTE)((c >> 24) & 0xFF); /* A */
            }
        }
        return S_OK;
    } else if (IsEqualGUID(pguidDstFormat, &GUID_WICPixelFormat8bppIndexed)) {
        if (nStride == 0) {
            nStride = rw;
        }
        if (cbBufferSize < nStride * rh) {
            return E_INVALIDARG;
        }
        src = f->indices + y * f->w + x;
        for (i = 0; i < rh; ++i) {
            memcpy(pbBuffer + i * nStride,
                   src + i * f->w,
                   rw);
        }
        return S_OK;
    }

    return WINCODEC_ERR_UNSUPPORTEDPIXELFORMAT;
}

/* IWICBitmapSourceTransform::GetClosestSize
 *
 * wincodec.h
 *
 * HRESULT GetClosestSize(
 *   [in, out] UINT *puiWidth,
 *   [in, out] UINT *puiHeight
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_Transform_GetClosestSize(
    IWICBitmapSourceTransform                 *iface,
    UINT                      /* [in, out] */ *puiWidth,
    UINT                      /* [in, out] */ *puiHeight)
{
    SixelFrame *f;

    f = (SixelFrame*)((BYTE*)iface - offsetof(SixelFrame, lpTransVtbl));

    if (puiWidth == NULL || puiHeight == NULL) {
        return E_INVALIDARG;
    }

    *puiWidth = f->w;
    *puiHeight = f->h;

    return S_OK;
}

/* IWICBitmapSourceTransform::GetClosestPixelFormat
 *
 * wincodec.h
 *
 * HRESULT GetClosestPixelFormat(
 *   [in, out] WICPixelFormatGUID *pPixelFormat
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_Transform_GetClosestPixelFormat(
    IWICBitmapSourceTransform                 *iface,
    WICPixelFormatGUID        /* [in, out] */ *pPixelFormat)
{
    (void) iface;

    if (! pPixelFormat) {
        return E_INVALIDARG;
    }

    if (IsEqualGUID(pPixelFormat, &GUID_WICPixelFormat32bppBGRA)) {
        *pPixelFormat = GUID_WICPixelFormat32bppBGRA;
    } else if (IsEqualGUID(pPixelFormat, &GUID_WICPixelFormat8bppIndexed)) {
        *pPixelFormat = GUID_WICPixelFormat8bppIndexed;
    }

    return S_OK;
}

/* IWICBitmapSourceTransform::DoesSupportTransform
 *
 * wincodec.h
 *
 * HRESULT DoesSupportTransform(
 *   [in]  WICBitmapTransformOptions dstTransform,
 *   [out] BOOL *pfIsSupported
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_Transform_DoesSupportTransform(
    IWICBitmapSourceTransform             *iface,
    WICBitmapTransformOptions /* [in] */   dstTransform,
    BOOL                      /* [out] */ *pfIsSupported)
{
    (void) iface;

    if (! pfIsSupported) {
        return E_INVALIDARG;
    }

    *pfIsSupported = (dstTransform == WICBitmapTransformRotate0);

    return S_OK;
}

static IWICBitmapSourceTransformVtbl SixelFrame_Transform_Vtbl = {
    SixelFrame_Transform_QueryInterface,
    SixelFrame_Transform_AddRef,
    SixelFrame_Transform_Release,
    SixelFrame_Transform_CopyPixels,
    SixelFrame_Transform_GetClosestSize,
    SixelFrame_Transform_GetClosestPixelFormat,
    SixelFrame_Transform_DoesSupportTransform
};

/* IIWICBitmapFrameDecode */

/* IWICBitmapFrameDecode::GetMetadataQueryReader
 *
 * wincodec.h
 *
 * HRESULT GetMetadataQueryReader(
 *   [out] IWICMetadataQueryReader **ppIMetadataQueryReader
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_GetMetadataQueryReader(
    IWICBitmapFrameDecode                 *iface,
    IWICMetadataQueryReader  /* [out] */ **pp)
{
    (void) iface;

    if (pp == NULL) {
        return E_INVALIDARG;
    }

    *pp = NULL;

    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

/* IWICBitmapFrameDecode::GetColorContexts
 *
 * wincodec.h
 *
 * HRESULT GetColorContexts(
 *   [in]      UINT             cCount,
 *   [in, out] IWICColorContext **ppIColorContexts,
 *   [out]     UINT             *pcActualCount
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_GetColorContexts(
    IWICBitmapFrameDecode                  *iface,
    UINT                  /* [in] */        cCount,
    IWICColorContext      /* [in, out] */ **ppIColorContexts,
    UINT                  /* [out] */      *pcActualCount)
{
    (void) iface;

    if (pcActualCount == NULL) {
        return E_INVALIDARG;
    }

    *pcActualCount = 1;

    if (ppIColorContexts == NULL) {
        if (cCount == 0) {
            return S_OK;
        } else {
            return E_INVALIDARG;
        }
    }

    return IWICColorContext_InitializeFromExifColorSpace(*ppIColorContexts,
                                                         EXIF_COLORSPACE_SRGB);
}

/* IWICBitmapFrameDecode::GetThumbnail
 *
 * wincodec.h
 *
 * HRESULT GetThumbnail(
 *   [out] IWICBitmapSource **ppIThumbnail
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelFrame_GetThumbnail(
    IWICBitmapFrameDecode               *iface,
    IWICBitmapSource       /* [out] */ **ppIThumbnail)
{
    (void) iface;

    if (ppIThumbnail == NULL) {
        return E_INVALIDARG;
    }

    *ppIThumbnail = NULL;

    return WINCODEC_ERR_CODECNOTHUMBNAIL;
}

/*
 * [
 *     object,
 *     uuid(00000120-a8f2-4877-ba0a-fd2b6645fb94)
 * ]
 * interface IWICBitmapSource : IUnknown
 * {
 *     HRESULT GetSize(
 *         [out] UINT *puiWidth,
 *         [out] UINT *puiHeight);
 *
 *     HRESULT GetPixelFormat(
 *         [out] WICPixelFormatGUID *pPixelFormat);
 *
 *     HRESULT GetResolution(
 *         [out] double *pDpiX,
 *         [out] double *pDpiY);
 *
 *     HRESULT CopyPalette(
 *         [in] IWICPalette *pIPalette);
 *
 *     HRESULT CopyPixels(
 *         [in] const WICRect *prc,
 *         [in] UINT cbStride,
 *         [in] UINT cbBufferSize,
 *         [out, size_is(cbBufferSize)] BYTE *pbBuffer);
 * }
 *
 * [
 *     object,
 *     uuid(3b16811b-6a43-4ec9-a813-3d930c13b940)
 * ]
 * interface IWICBitmapFrameDecode : IWICBitmapSource
 * {
 *     HRESULT GetMetadataQueryReader(
 *         [out] IWICMetadataQueryReader **ppIMetadataQueryReader);
 *
 *     HRESULT GetColorContexts(
 *         [in] UINT cCount,
 *         [in, out, unique, size_is(cCount)] IWICColorContext **ppIColorContexts,
 *         [out] UINT *pcActualCount);
 *
 *     HRESULT GetThumbnail(
 *         [out] IWICBitmapSource **ppIThumbnail);
 * }
 */
static IWICBitmapFrameDecodeVtbl SixelFrame_Vtbl = {
    /* IUnknown */
    SixelFrame_QueryInterface,
    SixelFrame_AddRef,
    SixelFrame_Release,
    /* IIWICBitmapSource */
    SixelFrame_GetSize,
    SixelFrame_GetPixelFormat,
    SixelFrame_GetResolution,
    SixelFrame_CopyPalette,
    SixelFrame_CopyPixels,
    /* IIWICBitmapFrameDecode */
    SixelFrame_GetMetadataQueryReader,
    SixelFrame_GetColorContexts,
    SixelFrame_GetThumbnail
};

/* Decoder object (implements IWICBitmapDecoder) */

typedef struct {
    const IWICBitmapDecoderVtbl *lpVtbl;  /* vtbl pointer */
    LONG ref;
    BOOL initialized;
    UINT w;
    UINT h;
    BYTE *indices;
    WICColor *palette;
    UINT ncolors;
} SixelDecoder;

static HRESULT STDMETHODCALLTYPE
SixelDecoder_QueryInterface(
    IWICBitmapDecoder              *iface,
    REFIID            /* [in]  */   riid,
    void              /* [out] */ **ppv
)
{
    if(ppv == NULL) {
        return E_POINTER;
    }

    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    } else if (IsEqualIID(riid, &IID_IWICBitmapDecoder)) {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
SixelDecoder_AddRef(
    IWICBitmapDecoder* iface
)
{
    SixelDecoder *decoder;

    decoder = (SixelDecoder*)iface;

    return (ULONG)InterlockedIncrement(&decoder->ref);
}

static ULONG STDMETHODCALLTYPE
SixelDecoder_Release(IWICBitmapDecoder* iface)
{
    SixelDecoder *decoder;
    ULONG n;

    decoder = (SixelDecoder*)iface;

    n = (ULONG)InterlockedDecrement(&decoder->ref);

    if(n == 0) {
        if (decoder->indices) {
            CoTaskMemFree(decoder->indices);
        }
        if (decoder->palette) {
            CoTaskMemFree(decoder->palette);
        }
        CoTaskMemFree(decoder);
    }
    return n;
}

/* IWICBitmapDecoder::QueryCapability
 *
 * wincodec.h
 *
 * HRESULT QueryCapability(
 *   [in]  IStream *pIStream,
 *   [out] DWORD   *pdwCapability
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_QueryCapability(
    IWICBitmapDecoder             *iface,
    IStream           /* [in]  */ *pIStream,
    DWORD             /* [out] */ *pdwCapability
)
{
    (void) iface;
    (void) pIStream;

    if (pdwCapability == NULL) {
        return E_INVALIDARG;
    }

    *pdwCapability = WICBitmapDecoderCapabilityCanDecodeAllImages
                   | WICBitmapDecoderCapabilityCanDecodeThumbnail
                   ;

    return S_OK;
}

/* IWICBitmapDecoder::Initialize
 *
 * wincodec.h
 *
 * HRESULT Initialize(
 *   [in] IStream          *pIStream,
 *   [in] WICDecodeOptions cacheOptions
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_Initialize(
    IWICBitmapDecoder              *iface,
    IStream             /* [in] */ *pIStream,
    WICDecodeOptions    /* [in] */  cacheOptions
)
{
    SixelDecoder *decoder;
    STATSTG stat;
    HRESULT hr = E_FAIL;
    ULONG size;
    ULONG read;
    SIXELSTATUS status = SIXEL_OK;
    sixel_allocator_t *allocator;
    unsigned char *input = NULL;
    unsigned char *pixels = NULL;
    unsigned char *palette = NULL;
    int width;
    int height;
    UINT i;
    int ncolors;

    (void) cacheOptions;  /* TODO: cache implementation */

    if (pIStream == NULL) {
        return E_POINTER;
    }

    hr = IStream_Stat(pIStream, &stat, STATFLAG_NONAME);
    if (FAILED(hr)) {
        return hr;
    }

    if (stat.cbSize.HighPart != 0) {
        return E_OUTOFMEMORY;
    }

    status = sixel_allocator_new(
        &allocator,
        wic_malloc,
        wic_calloc,
        wic_realloc,
        wic_free);
    if (SIXEL_FAILED(status)) {
        return E_FAIL;
    }

    size = stat.cbSize.LowPart;
    input = sixel_allocator_malloc(allocator, size);

    read = 0;
    hr = IStream_Read(pIStream, input, size, &read);
    if (FAILED(hr)) {
        return hr;
    }
    if (read != size) {
        hr = E_FAIL;
        goto end;
    }

    status = sixel_decode_raw(
        input, size, &pixels, &width, &height,
        &palette, &ncolors, allocator);
    if (SIXEL_FAILED(status)) {
        return E_FAIL;
    }

    decoder = (SixelDecoder*)iface;
    if (decoder->initialized) {
        return WINCODEC_ERR_WRONGSTATE;
    }

    decoder->w = width;
    decoder->h = height;
    decoder->indices = (BYTE*)CoTaskMemAlloc(width * height);
    decoder->palette = (WICColor*)CoTaskMemAlloc(ncolors * sizeof(WICColor));
    if (decoder->indices == NULL || decoder->palette == NULL) {
        hr = E_OUTOFMEMORY;
        goto end;
    }

    memcpy(decoder->indices, pixels, width * height);
    for (i = 0; i < (UINT)ncolors; ++i) {
        decoder->palette[i] = 0xFF000000 |
            (palette[i * 3 + 0] << 16) |
            (palette[i * 3 + 1] << 8) |
            (palette[i * 3 + 2]);
    }
    decoder->ncolors = ncolors;
    decoder->initialized = TRUE;
    hr = S_OK;

end:
    sixel_allocator_free(allocator, input);
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, palette);
    sixel_allocator_unref(allocator);

    return hr;
}

/* IWICBitmapDecoder::GetContainerFormat
 *
 * wincodec.h
 *
 * HRESULT GetContainerFormat(
 *   [out] GUID *pguidContainerFormat
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_GetContainerFormat(
    IWICBitmapDecoder               *iface,
    GUID                /* [out] */ *pguidContainerFormat
)
{
    (void) iface;

    if (pguidContainerFormat == NULL) {
        return E_INVALIDARG;
    }

    *pguidContainerFormat = GUID_ContainerFormatSIXEL;

    return S_OK;
}

/* IWICBitmapDecoder::GetDecoderInfo
 *
 * wincodec.h
 *
 * HRESULT GetDecoderInfo(
 *   [out] IWICBitmapDecoderInfo **ppIDecoderInfo
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_GetDecoderInfo(
    IWICBitmapDecoder                  *iface,
    IWICBitmapDecoderInfo /* [out] */ **ppIDecoderInfo
)
{
    IWICImagingFactory *fac = NULL;
    IWICComponentInfo *ci = NULL;
    HRESULT hr = E_FAIL;

    (void) iface;

    if (ppIDecoderInfo == NULL) {
        return E_INVALIDARG;
    }

    *ppIDecoderInfo = NULL;

    hr = CoCreateInstance(&CLSID_WICImagingFactory,
                          NULL,
                          CLSCTX_INPROC_SERVER,
                          &IID_IWICImagingFactory,
                          (void**)&fac);
    if (FAILED(hr)) return hr;

    hr = IWICImagingFactory_CreateComponentInfo(
        fac,
        &CLSID_SixelDecoder,
        &ci);

    IWICImagingFactory_Release(fac);

    if (FAILED(hr)) {
        return hr;
    }

    /* IWICBitmapDecoderInfo */
    hr = IWICComponentInfo_QueryInterface(
         ci,
         &IID_IWICBitmapDecoderInfo,
         (void**)ppIDecoderInfo);

    IWICComponentInfo_Release(ci);

    return hr;
}

/* IWICBitmapDecoder::CopyPalette
 *
 * wincodec.h
 *
 * HRESULT CopyPalette(
 *   [in] IWICPalette *pIPalette
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_CopyPalette(
    IWICBitmapDecoder            *iface,
    IWICPalette       /* [in] */ *pIPalette
)
{
    SixelDecoder *decoder;

    if (pIPalette == NULL) {
        return E_INVALIDARG;
    }

    decoder = (SixelDecoder*)iface;
    if (!decoder->initialized) {
        return WINCODEC_ERR_NOTINITIALIZED;
    }

    return IWICPalette_InitializeCustom(pIPalette, decoder->palette, decoder->ncolors);
}

/* IWICBitmapDecoder::GetMetadataQueryReader
 *
 * wincodec.h
 *
 * HRESULT GetMetadataQueryReader(
 *   [out] IWICMetadataQueryReader **ppIMetadataQueryReader
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_GetMetadataQueryReader(
    IWICBitmapDecoder                     *iface,
    IWICMetadataQueryReader  /* [out] */ **ppIMetadataQueryReader
)
{
    (void) iface;

    if (ppIMetadataQueryReader == NULL) {
        return E_INVALIDARG;
    }

    *ppIMetadataQueryReader = NULL;

    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

/* IWICBitmapDecoder::GetPreview
 *
 * wincodec.h
 *
 * HRESULT GetPreview(
 *   [out] IWICBitmapSource **ppIBitmapSource
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_GetPreview(
    IWICBitmapDecoder              *iface,
    IWICBitmapSource  /* [out] */ **ppIBitmapSource
)
{
    IWICBitmapFrameDecode *frame = NULL;
    HRESULT hr = E_FAIL;

    if (ppIBitmapSource == NULL) {
        return E_INVALIDARG;
    }

    hr = IWICBitmapDecoder_GetFrame(iface, 0, &frame);
    if (FAILED(hr)) {
        return hr;
    }

    *ppIBitmapSource = (IWICBitmapSource*)frame;

    return S_OK;
}

/* IWICBitmapDecoder::GetColorContexts
 *
 * wincodec.h
 *
 * HRESULT GetColorContexts(
 *   [in]      UINT             cCount,
 *   [in, out] IWICColorContext **ppIColorContexts,
 *   [out]     UINT             *pcActualCount
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_GetColorContexts(
    IWICBitmapDecoder                  *iface,
    UINT              /* [in]      */   cCount,
    IWICColorContext  /* [in, out] */ **ppIColorContexts,
    UINT              /* [out]     */  *pcActualCount
)
{
    (void) iface;
    (void) cCount;
    (void) ppIColorContexts;

    if (pcActualCount) {
        *pcActualCount = 0;
    }

    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
}

/* IWICBitmapDecoder::GetThumbnail
 *
 * wincodec.h
 *
 * HRESULT GetThumbnail(
 *   [out] IWICBitmapSource **ppIThumbnail
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_GetThumbnail(
    IWICBitmapDecoder              *iface,
    IWICBitmapSource  /* [out] */ **ppIThumbnail
)
{
    SixelDecoder *decoder;
    SixelFrame *frame;
    BYTE *thumb;
    WICColor *pal_copy;
    UINT tw, th;
    UINT x, y, sx, sy;

    if (ppIThumbnail == NULL) {
        return E_INVALIDARG;
    }

    decoder = (SixelDecoder*)iface;
    if (!decoder->initialized) {
        return WINCODEC_ERR_NOTINITIALIZED;
    }

    tw = decoder->w;
    th = decoder->h;
    if (tw >= th) {
        if (tw > 256) {
            th = (decoder->h * 256) / decoder->w;
            tw = 256;
        }
    } else {
        if (th > 256) {
            tw = (decoder->w * 256) / decoder->h;
            th = 256;
        }
    }

    thumb = (BYTE*)CoTaskMemAlloc(tw * th);
    if (thumb == NULL) {
        return E_OUTOFMEMORY;
    }

    for (y = 0; y < th; ++y) {
        sy = y * decoder->h / th;
        for (x = 0; x < tw; ++x) {
            sx = x * decoder->w / tw;
            thumb[y * tw + x] = decoder->indices[sy * decoder->w + sx];
        }
    }

    pal_copy = (WICColor*)CoTaskMemAlloc(decoder->ncolors * sizeof(WICColor));
    if (pal_copy == NULL) {
        CoTaskMemFree(thumb);
        return E_OUTOFMEMORY;
    }
    memcpy(pal_copy, decoder->palette, decoder->ncolors * sizeof(WICColor));

    frame = (SixelFrame*)CoTaskMemAlloc(sizeof(SixelFrame));
    if (frame == NULL) {
        CoTaskMemFree(thumb);
        CoTaskMemFree(pal_copy);
        return E_OUTOFMEMORY;
    }

    frame->lpVtbl = &SixelFrame_Vtbl;
    frame->lpTransVtbl = &SixelFrame_Transform_Vtbl;
    frame->ref = 1;
    frame->w = tw;
    frame->h = th;
    frame->indices = thumb;
    frame->palette = pal_copy;
    frame->ncolors = decoder->ncolors;

    *ppIThumbnail = (IWICBitmapSource*)frame;

    return S_OK;
}

/* IWICBitmapDecoder::GetFrameCount
 *
 * wincodec.h
 *
 * HRESULT GetFrameCount(
 *   [out] UINT *pCount
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_GetFrameCount(
    IWICBitmapDecoder              *iface,
    UINT               /* [out] */ *pCount
)
{
    SixelDecoder *decoder;

    (void) iface;

    decoder = (SixelDecoder*)iface;

    if (pCount == NULL) {
        return E_INVALIDARG;
    }

    *pCount = decoder->initialized ? 1: 0;

    return S_OK;
}

/* IWICBitmapDecoder::GetFrame
 *
 * wincodec.h
 *
 * HRESULT GetFrame(
 *   [in]  UINT                  index,
 *   [out] IWICBitmapFrameDecode **ppIBitmapFrame
 * );
 */
static HRESULT STDMETHODCALLTYPE
SixelDecoder_GetFrame(
    IWICBitmapDecoder                    *iface,
    UINT                   /* [in] */     index,
    IWICBitmapFrameDecode  /* [out] */  **ppIBitmapFrame
)
{
    SixelDecoder *decoder;
    SixelFrame *frame;
    BYTE *idx_copy;
    WICColor *pal_copy;

    decoder = (SixelDecoder*)iface;

    if (ppIBitmapFrame == NULL) {
        return E_INVALIDARG;
    }

    *ppIBitmapFrame = NULL;

    if(! decoder->initialized) {
        return WINCODEC_ERR_NOTINITIALIZED;
    }

    if (index != 0) {
        return WINCODEC_ERR_FRAMEMISSING;
    }

    idx_copy = (BYTE*)CoTaskMemAlloc(decoder->w * decoder->h);
    pal_copy = (WICColor*)CoTaskMemAlloc(decoder->ncolors * sizeof(WICColor));

    if (idx_copy == NULL || pal_copy == NULL) {
        CoTaskMemFree(idx_copy);
        CoTaskMemFree(pal_copy);
        return E_OUTOFMEMORY;
    }

    memcpy(idx_copy, decoder->indices, decoder->w * decoder->h);
    memcpy(pal_copy, decoder->palette, decoder->ncolors * sizeof(WICColor));

    frame = (SixelFrame*)CoTaskMemAlloc(sizeof(SixelFrame));
    if(frame == NULL) {
        CoTaskMemFree(idx_copy);
        CoTaskMemFree(pal_copy);
        return E_OUTOFMEMORY;
    }

    frame->lpVtbl = &SixelFrame_Vtbl;
    frame->lpTransVtbl = &SixelFrame_Transform_Vtbl;
    frame->ref = 1;
    frame->w = decoder->w;
    frame->h = decoder->h;
    frame->indices = idx_copy;
    frame->palette = pal_copy;
    frame->ncolors = decoder->ncolors;
    *ppIBitmapFrame = (IWICBitmapFrameDecode *)frame;

    return S_OK;
}

static IWICBitmapDecoderVtbl SixelDecoder_Vtbl = {
    /* IUnknown */
    SixelDecoder_QueryInterface,
    SixelDecoder_AddRef,
    SixelDecoder_Release,
    /* IWICBitmapDecoder */
    SixelDecoder_QueryCapability,
    SixelDecoder_Initialize,
    SixelDecoder_GetContainerFormat,
    SixelDecoder_GetDecoderInfo,
    SixelDecoder_CopyPalette,
    SixelDecoder_GetMetadataQueryReader,
    SixelDecoder_GetPreview,
    SixelDecoder_GetColorContexts,
    SixelDecoder_GetThumbnail,
    SixelDecoder_GetFrameCount,
    SixelDecoder_GetFrame
};

/* ClassFactory (IClassFactory) */
typedef struct {
    const IClassFactoryVtbl* lpVtbl;
    LONG ref;
} SixelFactory;

static LONG g_serverLocks = 0;

static HRESULT STDMETHODCALLTYPE
SixelFactory_QueryInterface(
    IClassFactory               *iface,
    REFIID         /* [in] */    riid,
    void           /* [out] */ **ppv
)
{
    if (ppv == NULL) {
        return E_POINTER;
    }

    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown)) {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    } else if (IsEqualIID(riid, &IID_IClassFactory)) {
        *ppv = iface;
        IUnknown_AddRef(iface);
        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
SixelFactory_AddRef(
    IClassFactory *iface
)
{
    SixelFactory *factory;

    factory = (SixelFactory*)iface;

    return (ULONG)InterlockedIncrement(&factory->ref);
}

static ULONG STDMETHODCALLTYPE
SixelFactory_Release(
    IClassFactory *iface
)
{
    SixelFactory *factory;
    ULONG n;

    factory = (SixelFactory*)iface;

    n = (ULONG)InterlockedDecrement(&factory->ref);
    if (n == 0) {
        CoTaskMemFree(factory);
    }

    return n;
}

static HRESULT STDMETHODCALLTYPE
SixelFactory_CreateInstance(
    IClassFactory *iface,
    IUnknown      *outer,
    REFIID         riid,
    void         **ppv
)
{
    SixelDecoder *decoder;
    HRESULT hr = E_FAIL;

    (void) iface;

    if (outer) {
        return CLASS_E_NOAGGREGATION;
    }

    decoder = (SixelDecoder*)CoTaskMemAlloc(sizeof(SixelDecoder));
    if (decoder == NULL) {
        return E_OUTOFMEMORY;
    }

    decoder->lpVtbl = &SixelDecoder_Vtbl;
    decoder->ref = 1;
    decoder->initialized = FALSE;
    decoder->w = 0;
    decoder->h = 0;
    decoder->indices = NULL;
    decoder->palette = NULL;
    decoder->ncolors = 0;

    hr = IUnknown_QueryInterface((IWICBitmapDecoder*)decoder, riid, ppv);
    SixelDecoder_Release((IWICBitmapDecoder*)decoder);

    return hr;
}

static HRESULT STDMETHODCALLTYPE
SixelFactory_LockServer(
    IClassFactory *factory,
    BOOL           fLock
)
{
    (void) factory;

    if (fLock) {
        InterlockedIncrement(&g_serverLocks);
    } else {
        InterlockedDecrement(&g_serverLocks);
    }

    return S_OK;
}

static IClassFactoryVtbl SixelFactory_Vtbl = {
    /* IUnknown */
    SixelFactory_QueryInterface,
    SixelFactory_AddRef,
    SixelFactory_Release,
    /* IClassFactory */
    SixelFactory_CreateInstance,
    SixelFactory_LockServer
};

/* Registry helpers */
static void
RegisterStringValue(
    HKEY root,
    const LPCWSTR subkey,
    const LPCWSTR name,
    const LPCWSTR value
)
{
    HKEY h;
    LONG r;

    r = RegCreateKeyExW(root, subkey, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &h, NULL);

    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegCreateKeyExW: failed. key: %ls, code: %lu\n", subkey, r);
        return;
    }

    r = RegSetValueExW(h,
                       name,
                       0,
                       REG_SZ,
                       (const BYTE*)value,
                       (DWORD)((lstrlenW(value) + 1) * sizeof(WCHAR)));

    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegSetValueExW: failed. key: %ls, code: %lu\n",
                 subkey, r);
        return;
    }

    r = RegCloseKey(h);
    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegCloseKey: failed. key: %ls, code: %lu\n",
                 subkey, r);
        return;
    }

}

static void
RegisterDwordValue(
    HKEY root,
    LPCWSTR subkey,
    LPCWSTR name,
    DWORD value
)
{
    HKEY h;
    LONG r;

    r = RegCreateKeyExW(root, subkey, 0, NULL, 0,
                        KEY_SET_VALUE, NULL, &h, NULL);
    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegCreateKeyExW: failed. key: %ls, code: %lu\n",
                 subkey, r);
        return;
    }

    r = RegSetValueExW(h, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));
    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegSetValueExW: failed. key: %ls, name: %ls, code: %lu\n",
                 subkey, name, r);
        return;
    }

    r = RegCloseKey(h);
    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegCloseKey: failed. key: %ls, code: %lu\n",
                 subkey, r);
        return;
    }
}

static void
RegisterBinaryValue(
    HKEY root,
    LPCWSTR subkey,
    LPCWSTR name,
    LPCVOID data,
    DWORD cb
)
{
    HKEY h;
    LONG r;

    r = RegCreateKeyExW(root, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &h, NULL);
    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegCreateKeyExW: failed. key: %ls, code: %lu\n",
                 subkey, r);
        return;
    }

    r = RegSetValueExW(h, name, 0, REG_BINARY, (const BYTE*)data, cb);
    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegSetValueExW: failed. key: %ls, name: %ls, code: %lu\n",
                 subkey, name, r);
        return;
    }

    r = RegCloseKey(h);
    if (r != ERROR_SUCCESS) {
        fwprintf(stderr,
                 L"RegCloseKey: failed. key: %ls, code: %lu\n",
                 subkey, r);
        return;
    }
}


/* DLL exports */
__declspec(dllexport)
STDAPI
DllRegisterServer(void)
{
    const BYTE pat[]  = { 0x1B, 0x50 };   /* ESC 'P' */
    const BYTE mask[] = { 0xFF, 0xFF };
    LPWSTR key = NULL;
    WCHAR modulePath[MAX_PATH];

    GetModuleFileNameW((HINSTANCE)&__ImageBase, modulePath, MAX_PATH);

    /* extensions */

    /*
     * extension: .six
     */
    key = L".six";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, L"sixelfile");  /* progid */
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"ContentType", L"image/x-sixel");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"PerceivedType", L"image");

    /* for windows photo viewer */
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"OpenWithProgids", L"sixelfile");  /* progid */
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"OpenWithList", L"PhotoViewer.dll");

    key = L".six\\ShellEx\\ContextMenuHandlers\\ShellImagePreview";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, CLSIDSTR_WindowsPhotoViewer);

    /* IThumbnailProvider */
    key = L".six\\ShellEx\\" IIDSTR_IThumbnailProvider;
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, CLSIDSTR_PhotoThumbnailProvider);

    key = L"SystemFileAssociations\\.six";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"OpenWithList", L"PhotoViewer.dll");

    key = L"SystemFileAssociations\\.six\\ShellEx\\"
          L"ContextMenuHandlers\\ShellImagePreview";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, CLSIDSTR_WindowsPhotoViewer);

    /* IThumbnailProvider */
    key = L"SystemFileAssociations\\.six\\ShellEx\\"
          IIDSTR_IThumbnailProvider;
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, CLSIDSTR_PhotoThumbnailProvider);

    /* System.Kind support */
    key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap";
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L".six", L"Picture");

    /*
     * extension: .sixel
     */
    key = L".sixel";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, L"sixelfile");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"ContentType", L"image/x-sixel");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"PerceivedType", L"image");

    /* for windows photo viewer */
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"OpenWithProgids", L"sixelfile");  /* progid */
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"OpenWithList", L"PhotoViewer.dll");

    key = L".sixel\\ShellEx\\ContextMenuHandlers\\ShellImagePreview";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, CLSIDSTR_WindowsPhotoViewer);

    key = L"SystemFileAssociations\\.sixel";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"OpenWithList", L"PhotoViewer.dll");

    key = L"SystemFileAssociations\\.sixel\\ShellEx\\"
          L"ContextMenuHandlers\\ShellImagePreview";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, CLSIDSTR_WindowsPhotoViewer);

    key = L"SystemFileAssociations\\.sixel\\ShellEx\\"
          IIDSTR_IThumbnailProvider;
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, CLSIDSTR_PhotoThumbnailProvider);

    /* System.Kind support */
    key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap";
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L".sixel", L"Picture");

    /*
     * progid: "sixelfile"
     */
    key = L"sixelfile";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, L"SIXEL image format");

    key = L"sixelfile\\open";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"MuiVerb",
                        L"@%PROGRAMFILES%\\Windows Photo Viewer\\"
                        L"PhotoViewer.dll,-3043");

    key = L"sixelfile\\shell\\open\\command";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL,
                        L"%SystemRoot%\\System32\\rundll32.exe"
                        L" \"%ProgramFiles%\\Windows Photo Viewer\\"
                           L"PhotoViewer.dll\","
                        L" ImageView_Fullscreen %1");

    key = L"sixelfile\\shell\\open\\DropTarget";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"Clsid", CLSIDSTR_WindowsPhotoViewer);

    key = L"sixelfile\\shell\\printto\\command";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, L"%SystemRoot%\\System32\\rundll32.exe"
                              L" \"%SystemRoot%\\System32\\shimgvw.dll\","
                              L" ImageView_PrintTo"
                              L" /pt \"%1\" \"%2\" \"%3\" \"%4\"");

    /*
     * CLSID
     */
    key = L"CLSID\\" CLSIDSTR_SixelDecoder;
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, L"WIC SIXEL Decoder");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"FriendlyName", L"WIC SIXEL Decoder");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"Description", L"Decoder for DEC SIXEL graphics");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"Author", L"Hayaki Saito");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"CLSID", CLSIDSTR_SixelDecoder);
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"VendorGUID", GUIDSTR_VendorSIXEL);
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"ContainerFormat", GUIDSTR_ContainerFormatSIXEL);
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"FileExtensions", L".six;.sixel");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"MimeTypes", L"image/x-sixel");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"Version", L"1.0.0.1");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"Date", L"2014-02-20");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"SpecVersion", L"1.0.0.0");
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"ColorManagementVersion", L"1.0");
    RegisterDwordValue (HKEY_CLASSES_ROOT, key,
                        L"SupportsAnimation", 0);
    RegisterDwordValue (HKEY_CLASSES_ROOT, key,
                        L"SupportsChromakey", 0);
    RegisterDwordValue (HKEY_CLASSES_ROOT, key,
                        L"SupportsLossless", 1);
    RegisterDwordValue (HKEY_CLASSES_ROOT, key,
                        L"SupportsMultiframe", 0);
    RegisterDwordValue (HKEY_CLASSES_ROOT, key,
                        L"ArbitrationPriority", 0x0);
    RegisterDwordValue (HKEY_CLASSES_ROOT, key,
                        L"Capabilities", 0x9);

    /* InprocServer32 */
    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\"
          L"InprocServer32";
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, modulePath);
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"ThreadingModel", L"Both");

    /* Formats */
    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\"
          L"Formats\\" GUIDSTR_WICPixelFormat8bppIndexed;
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, L"");
    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\"
          L"Formats\\" GUIDSTR_WICPixelFormat32bppBGRA;
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        NULL, L"");

    /* Patterns */
    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\"
          L"Patterns\\0";
    RegisterDwordValue (HKEY_CLASSES_ROOT, key,
                        L"Position", 0);
    RegisterDwordValue (HKEY_CLASSES_ROOT, key,
                        L"Length", 2);
    RegisterBinaryValue(HKEY_CLASSES_ROOT, key,
                        L"Pattern", pat, sizeof(pat));
    RegisterBinaryValue(HKEY_CLASSES_ROOT, key,
                        L"Mask", mask, sizeof(mask));

    /* category */
    key = L"CLSID\\" CATIDSTR_WICBitmapDecoders "\\"
          L"Instance\\" CLSIDSTR_SixelDecoder;
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"CLSID", CLSIDSTR_SixelDecoder);
    RegisterStringValue(HKEY_CLASSES_ROOT, key,
                        L"FriendlyName", L"WIC SIXEL Decoder");

    /* WIC extensions */
    key = L"Software\\Microsoft\\Windows Imaging Component\\"
          L"Extensions\\.six";
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"CLSID", CLSIDSTR_SixelDecoder);
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"ContainerFormat", GUIDSTR_ContainerFormatSIXEL);

    key = L"Software\\Microsoft\\Windows Imaging Component\\"
          L"Extensions\\.sixel";
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"CLSID", CLSIDSTR_SixelDecoder);
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"ContainerFormat", GUIDSTR_ContainerFormatSIXEL);

    /* WIC decoders */
    key = L"Software\\Microsoft\\Windows Imaging Component\\"
          L"Decoders\\" CLSIDSTR_SixelDecoder;
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        NULL, L"WIC SIXEL Decoder");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"FriendlyName", L"WIC SIXEL Decoder");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"Description", L"Decoder for DEC SIXEL graphics");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"Author", L"Hayaki Saito");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"CLSID", CLSIDSTR_SixelDecoder);
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"VendorGUID", GUIDSTR_VendorSIXEL);
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"ContainerFormat", GUIDSTR_ContainerFormatSIXEL);
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"FileExtensions", L".six");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"MimeTypes", L"image/x-sixel");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"Version", L"1.0.0.1");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"Date", L"2014-02-20");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"SpecVersion", L"1.0.0.0");
    RegisterStringValue(HKEY_LOCAL_MACHINE, key,
                        L"ColorManagementVersion", L"1.0");
    RegisterDwordValue (HKEY_LOCAL_MACHINE, key,
                        L"SupportsAnimation", 0);
    RegisterDwordValue (HKEY_LOCAL_MACHINE, key,
                        L"SupportsChromakey", 0);
    RegisterDwordValue (HKEY_LOCAL_MACHINE, key,
                        L"SupportsLossless", 1);
    RegisterDwordValue (HKEY_LOCAL_MACHINE, key,
                        L"SupportsMultiframe", 0);
    RegisterDwordValue (HKEY_LOCAL_MACHINE, key,
                        L"ArbitrationPriority", 0x0);
    RegisterDwordValue (HKEY_LOCAL_MACHINE, key,
                        L"Capabilities", 0x9);

#if WIC_DISABLE_THUMBNAIL_CACHE
    /* debugging: disable explorer's thumbnail cache
     * NOTE: please remove this registry value manually
     *       command:
     *         $ reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced\DisableThumbnailCache"
     */
    key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
    RegisterDwordValue (HKEY_CURRENT_USER, key,
                        L"DisableThumbnailCache", 1);
#endif
    return S_OK;
}


__declspec(dllexport)
STDAPI
DllUnregisterServer(void)
{
    LSTATUS r;
    LPWSTR key = NULL;

    /* WIC decoders */
    key = L"Software\\Microsoft\\Windows Imaging Component\\"
          L"Decoders\\" CLSIDSTR_SixelDecoder;
    r = RegDeleteKeyW(HKEY_LOCAL_MACHINE, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    /* WIC extensions */
    key = L"Software\\Microsoft\\Windows Imaging Component\\"
          L"Extensions\\.six";
    r = RegDeleteKeyW(HKEY_LOCAL_MACHINE, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"Software\\Microsoft\\Windows Imaging Component\\"
          L"Extensions\\.sixel";
    r = RegDeleteKeyW(HKEY_LOCAL_MACHINE, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    /* category */
    key = L"CLSID\\" CATIDSTR_WICBitmapDecoders L"\\"
          L"Instance\\" CLSIDSTR_SixelDecoder;
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    /* Patterns */
    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\Patterns\\0";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\Patterns";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    /* Formats */
    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\Formats\\"
          GUIDSTR_WICPixelFormat8bppIndexed;
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\Formats\\"
          GUIDSTR_WICPixelFormat32bppBGRA;
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"CLSID\\" CLSIDSTR_SixelDecoder L"\\Formats";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    /* InprocServer32 */
    key =L"CLSID\\" CLSIDSTR_SixelDecoder L"\\InprocServer32";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    /* CLSID */
    key = L"CLSID\\" CLSIDSTR_SixelDecoder;
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    /* progid */
    key = L"sixelfile\\printto\\command";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"sixelfile\\printto";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"sixelfile\\shell\\open\\DropTarget";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"sixelfile\\shell\\open\\command";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"sixelfile\\open";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"sixelfile";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    /* extensions */

    key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap";
    r = RegDeleteKeyW(HKEY_LOCAL_MACHINE, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.six\\ShellEx\\"
          IIDSTR_IThumbnailProvider;
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.six\\ShellEx\\"
          L"ContextMenuHandlers\\ShellImagePreview";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.six\\ShellEx\\"
          L"ContextMenuHandlers";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.six\\ShellEx";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.six";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".six\\ShellEx\\" IIDSTR_IThumbnailProvider;
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".six\\ShellEx\\ContextMenuHandlers\\ShellImagePreview";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".six\\ShellEx\\ContextMenuHandlers";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".six\\ShellEx";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".six";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.sixel\\ShellEx\\"
          IIDSTR_IThumbnailProvider;
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.sixel\\ShellEx\\"
          L"ContextMenuHandlers\\ShellImagePreview";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.sixel\\ShellEx\\"
          L"ContextMenuHandlers";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.sixel\\ShellEx";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L"SystemFileAssociations\\.sixel";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".sixel\\ShellEx\\" IIDSTR_IThumbnailProvider;
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".sixel\\ShellEx\\ContextMenuHandlers\\ShellImagePreview";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".sixel\\ShellEx\\ContextMenuHandlers";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".sixel\\ShellEx";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    key = L".sixel";
    r = RegDeleteKeyW(HKEY_CLASSES_ROOT, key);
    if (r != ERROR_SUCCESS && r != ERROR_FILE_NOT_FOUND) {
        fwprintf(stderr, L"RegDeleteKeyW: failed. key: %ls, code: %lu\n", key, r);
    }

    return S_OK;
}

#if defined(_MSC_VER)
_Check_return_
#else
__declspec(dllexport)
#endif
STDAPI
DllGetClassObject(
    REFCLSID   rclsid,
    REFIID     riid,
    void     **ppv
)
{
    SixelFactory *factory;
    HRESULT hr = E_FAIL;

    if (ppv == NULL) {
        return E_POINTER;
    }
    *ppv = NULL;

    if (IsEqualCLSID(rclsid, &CLSID_SixelDecoder)) {
        factory = (SixelFactory*)CoTaskMemAlloc(sizeof(SixelFactory));
        if (factory == NULL) {
            return E_OUTOFMEMORY;
        }

        factory->lpVtbl = &SixelFactory_Vtbl;
        factory->ref = 1;
        hr = IUnknown_QueryInterface((IClassFactory*)factory, riid, ppv);
        SixelFactory_Release((IClassFactory*)factory);

        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

#if defined(_MSC_VER)
__control_entrypoint(DllExport)
#else
__declspec(dllexport)
#endif
STDAPI
DllCanUnloadNow(void)
{
    return g_serverLocks == 0 ? S_OK: S_FALSE;
}

BOOL WINAPI
DllMain(
    HINSTANCE hinstDLL,  /* handle to DLL module */
    DWORD fdwReason,     /* reason for calling function */
    LPVOID lpvReserved   /* reserved */
)
{
    (void) lpvReserved;

    if (fdwReason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinstDLL);
    }

    return TRUE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
