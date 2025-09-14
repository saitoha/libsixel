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
#define _UNICODE
#define UNICODE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <initguid.h>
#include <objbase.h>
#include <strsafe.h>

#include <sixel.h>

extern IMAGE_DOS_HEADER __ImageBase;

/* custom malloc */
void *
wic_malloc(size_t size) {
    return CoTaskMemAlloc(size);
}

/* custom realloc */
void *
wic_realloc(void* ptr, size_t new_size)
{
    return CoTaskMemRealloc(ptr, new_size);
}

/* custom calloc */
void *
wic_calloc(size_t count, size_t size)
{
    size_t total;
    void *p;

    total = count * size;
    p = CoTaskMemAlloc(total);
    if (p) {
        memset(p, 0, total);
    }
    return p;
}

/* custom free */
void
wic_free(void* ptr)
{
    if (ptr) {
        CoTaskMemFree(ptr);
    }
}

/* CLSID */
static const CLSID CLSID_SixelDecoder = {
    /* 15b9b4da-b155-4977-8571-cf005884bcb9 */
    0x15b9b4da,
    0xb155,
    0x4977,
    { 0x85, 0x71, 0xcf, 0x00, 0x58, 0x84, 0xbc, 0xb9 }
};
static wchar_t const CLSID_SixelDecoder_String[]
    = L"{15B9B4DA-B155-4977-8571-CF005884BCB9}";

/* ContainerFormat */
static const GUID GUID_ContainerFormatSIXEL = {
    /* 5b2053a9-7a2e-4e0e-9d4c-96532364401a */
    0x5b2053a9,
    0x7a2e,
    0x4e0e,
    { 0x9d, 0x4c, 0x96, 0x53, 0x23, 0x64, 0x40, 0x1a }
};
static wchar_t const GUID_ContainerFormatSIXEL_String[]
    = L"{5B2053A9-7A2E-4E0E-9D4C-96532364401A}";

/* VendorGUID */
#if 0
static const GUID GUID_VendorSIXEL = {
    /* 0b0a6d1e-0c4f-42e9-ba37-1ff5636e9a55 */
    0x0b0a6d1e,
    0x0c4f,
    0x42e9,
    {0xba, 0x37, 0x1f, 0xf5, 0x63, 0x6e, 0x9a, 0x55}
};
#endif
static wchar_t const GUID_VendorSIXEL_String[]
    = L"{0B0A6D1E-0C4F-42E9-BA37-1FF5636E9A55}";

/* GUID_WICPixelFormat32bppBGRA */
static wchar_t const GUID_WICPixelFormat32bppBGRA_String[]
    = L"{6FDDC324-4E03-4BFE-B185-3D77768DC90F}";

/* IID_IUnknown  {00000000-0000-0000-C000-000000000046} */
const IID IID_IUnknown = 
{
    0x00000000,
    0x0000,
    0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

/* IID_IClassFactory {00000001-0000-0000-C000-000000000046} */
const IID IID_IClassFactory = 
{
    0x00000001,
    0x0000,
    0x0000,
    { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

static wchar_t const CATID_WICBitmapDecoders_String[]
    = L"{7ED96837-96F0-4812-B211-F13C24117ED3}";

/* Frame object (implements IWICBitmapFrameDecode) */
typedef struct {
    const IWICBitmapFrameDecodeVtbl* lpVtbl;  /* vtbl pointer */
    LONG ref;
    UINT w;
    UINT h;
    BYTE *buf;
} SixelFrame;

/* IUnknown */
static HRESULT STDMETHODCALLTYPE
SixelFrame_QueryInterface(
    IWICBitmapFrameDecode *iface,
    REFIID                 riid,
    void                 **ppv
)
{
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
        if (f->buf) {
            CoTaskMemFree(f->buf);
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

    *pPixelFormat = GUID_WICPixelFormat32bppBGRA;

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

    if(pDpiX == NULL || pDpiY == NULL) {
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
    (void) iface;
    (void) pIPalette;

    return WINCODEC_ERR_PALETTEUNAVAILABLE;
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

    f = (SixelFrame*)iface;
    if(dst == NULL) {
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

    if(cbStride == 0) {
        cbStride = rw * 4;
    }

    if(cbBufferSize < cbStride * rh) {
        return E_INVALIDARG;
    }

    src = f->buf + y * f->w * 4 + x * 4;
    for(i = 0; i < rh; ++i) {
        memcpy(dst + i * cbStride,
               src + i * f->w * 4,
               rw * 4);
    }

    return S_OK;
}

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

    *pp = NULL;

    return E_NOTIMPL;
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
    (void) cCount;
    (void) ppIColorContexts;

    if (pcActualCount == NULL) {
        return E_INVALIDARG;
    }

    *pcActualCount = 0;

    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
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
    BYTE* solid;
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
        if (decoder->solid) {
            CoTaskMemFree(decoder->solid);
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

    *pdwCapability = WICBitmapDecoderCapabilityCanDecodeAllImages;

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
    UINT x;
    UINT y;
    BYTE *p;
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
    int index;
    int ncolors;

    (void) cacheOptions;

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
    decoder->solid = (BYTE*)CoTaskMemAlloc(width * height * 4);
    if (decoder->solid == NULL) {
        return E_OUTOFMEMORY;
    }

    for(y = 0; y < decoder->h; ++y) {
        for(x = 0; x < decoder->w; ++x) {
            index = pixels[y * decoder->w + x];
            p = decoder->solid + y * decoder->w * 4 + x * 4;
            /* RGB -> BGRA */
            p[0] = palette[index * 3 + 2];  /* B */
            p[1] = palette[index * 3 + 1];  /* G */
            p[2] = palette[index * 3 + 0];  /* R */
            p[3] = 255;                     /* A */
        }
    }

    decoder->initialized = TRUE;

end:
    sixel_allocator_free(allocator, input);
    sixel_allocator_free(allocator, pixels);
    sixel_allocator_free(allocator, palette);
    sixel_allocator_unref(allocator);

    return S_OK;
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
    (void) iface;
    (void) pIPalette;

    return WINCODEC_ERR_PALETTEUNAVAILABLE;
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
        return E_NOTIMPL;
    }

    *ppIMetadataQueryReader = NULL;

    return E_NOTIMPL;
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
    (void) iface;
    (void) ppIBitmapSource;

    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
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
    (void) iface;
    (void) ppIThumbnail;

    return WINCODEC_ERR_UNSUPPORTEDOPERATION;
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
    BYTE *copy;

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

    copy = (BYTE*)CoTaskMemAlloc(decoder->w * decoder->h * 4);

    if (copy == NULL) {
        return E_OUTOFMEMORY;
    }

    memcpy(copy, decoder->solid, decoder->w * decoder->h * 4);

    frame = (SixelFrame*)CoTaskMemAlloc(sizeof(SixelFrame));
    if(frame == NULL) {
        CoTaskMemFree(copy);
        return E_OUTOFMEMORY;
    }

    frame->lpVtbl = &SixelFrame_Vtbl;
    frame->ref = 1;
    frame->w = decoder->w;
    frame->h = decoder->h;
    frame->buf = copy;
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
    IClassFactory *iface,
    REFIID         riid,
    void         **ppv
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
    } else if (IsEqualIID(riid,&IID_IClassFactory)) {
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
    decoder->solid = NULL;

    hr = SixelDecoder_QueryInterface((IWICBitmapDecoder*)decoder, riid, ppv);
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
    SixelFactory_QueryInterface,
    SixelFactory_AddRef,
    SixelFactory_Release,
    /* IClassFactory */
    SixelFactory_CreateInstance,
    SixelFactory_LockServer
};

/* DLL exports */
__declspec(dllexport)
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
        hr = SixelFactory_QueryInterface((IClassFactory*)factory, riid, ppv);
        SixelFactory_Release((IClassFactory*)factory);

        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

__declspec(dllexport)
STDAPI
DllCanUnloadNow(void)
{
    return g_serverLocks == 0 ? S_OK: S_FALSE;
}

BOOL APIENTRY
DllMain(
    HINSTANCE h,
    DWORD r,
    LPVOID
)
{
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
    }
    return TRUE;
}

/* Registry helpers & Register/Unregister */
static HRESULT
RegisterStringValue(
    HKEY root,
    const wchar_t *subkey,
    const wchar_t *name,
    const wchar_t *value
)
{
    HKEY h;
    LONG r;

    r = RegCreateKeyExW(root, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &h, NULL);

    if (r != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(r);
    }

    r = RegSetValueExW(h, name, 0, REG_SZ, (const BYTE*)value, (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));

    RegCloseKey(h);

    return r == ERROR_SUCCESS ? S_OK: HRESULT_FROM_WIN32(r);
}

static HRESULT
RegisterDwordValue(
    HKEY root,
    const wchar_t *subkey,
    const wchar_t *name,
    DWORD value
)
{
    HKEY h;
    LONG r;

    r = RegCreateKeyExW(root, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &h, NULL);
    if (r != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(r);
    }
    r = RegSetValueExW(h, name, 0, REG_DWORD, (const BYTE*)&value, sizeof(value));

    RegCloseKey(h);

    return r == ERROR_SUCCESS ? S_OK: HRESULT_FROM_WIN32(r);
}

static HRESULT
RegisterBinaryValue(
    HKEY root,
    const wchar_t *subkey,
    const wchar_t *name,
    const void *data,
    DWORD cb
)
{
    HKEY h;
    LONG r;

    r = RegCreateKeyExW(root, subkey, 0, NULL, 0, KEY_SET_VALUE, NULL, &h, NULL);

    if (r != ERROR_SUCCESS) {
        return HRESULT_FROM_WIN32(r);
    }
    r = RegSetValueExW(h, name, 0, REG_BINARY, (const BYTE*)data, cb);

    RegCloseKey(h);

    return r == ERROR_SUCCESS ? S_OK: HRESULT_FROM_WIN32(r);
}

static void
RegisterCodecKeysInCLSID(const wchar_t* clsidStr, const wchar_t* modulePath)
{
    /* CLSID */
    wchar_t clsidKey[256];
    wsprintfW(clsidKey, L"CLSID\\%s", clsidStr);

    /* InprocServer32 */
    {
        wchar_t inprocKey[300];
        wsprintfW(inprocKey, L"%s\\InprocServer32", clsidKey);
        RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey,  NULL,                 L"WIC SIXEL Decoder");
        RegisterStringValue(HKEY_CLASSES_ROOT, inprocKey, NULL,                 modulePath);
        RegisterStringValue(HKEY_CLASSES_ROOT, inprocKey, L"ThreadingModel",    L"Both");
    }

    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"FriendlyName",           L"WIC SIXEL Decoder (Dummy)");
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"Description",            L"WIC SIXEL Decoder (Dummy)");
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"Author",                 L"Hayaki Saito");
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"VendorGUID",             GUID_VendorSIXEL_String);
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"ContainerFormat",        GUID_ContainerFormatSIXEL_String);
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"FileExtensions",         L".six");
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"MimeTypes",              L"image/sixel");
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"Version",                L"1.0.0.0");
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"Date",                   L"2014-02-20");
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"SpecVersion",            L"1.0.0.0");
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, L"ColorManagementVersion", L"0");
    RegisterDwordValue (HKEY_CLASSES_ROOT, clsidKey, L"SupportsAnimation",      0);
    RegisterDwordValue (HKEY_CLASSES_ROOT, clsidKey, L"SupportsChromakey",      1);
    RegisterDwordValue (HKEY_CLASSES_ROOT, clsidKey, L"SupportsLossless",       1);
    RegisterDwordValue (HKEY_CLASSES_ROOT, clsidKey, L"SupportsMultiframe",     0);
    RegisterDwordValue (HKEY_CLASSES_ROOT, clsidKey, L"ArbitrationPriority",    0x100);

    /* Formats */
    {
        wchar_t formatsKey[512];
 
        wsprintfW(formatsKey, L"%s\\Formats\\%s", clsidKey, GUID_WICPixelFormat32bppBGRA_String);
        RegisterStringValue(HKEY_CLASSES_ROOT, formatsKey, NULL, L"");
    }

    /* Patterns */
    {
        wchar_t patKey[512];
        const BYTE pat[]  = { 0x1B, 0x50 };   /* ESC 'P' */
        const BYTE mask[] = { 0xFF, 0xFF };

        wsprintfW(patKey, L"%s\\Patterns\\0", clsidKey);
        RegisterDwordValue (HKEY_CLASSES_ROOT, patKey, L"Position", 0);
        RegisterDwordValue (HKEY_CLASSES_ROOT, patKey, L"Length",   2);
        RegisterBinaryValue(HKEY_CLASSES_ROOT, patKey, L"Pattern",  pat,  sizeof(pat));
        RegisterBinaryValue(HKEY_CLASSES_ROOT, patKey, L"Mask",     mask, sizeof(mask));
        /* RegisterDwordValue(HKEY_CLASSES_ROOT, patKey, L"EndOfStream", 0); */
    }

    /* category */
    {
        wchar_t instKey[512];

        wsprintfW(instKey,
                  L"CLSID\\%s\\Instance\\%s",
                  CATID_WICBitmapDecoders_String,
                  clsidStr);
        RegisterStringValue(HKEY_CLASSES_ROOT, instKey, L"CLSID",        clsidStr);
        RegisterStringValue(HKEY_CLASSES_ROOT, instKey, L"FriendlyName", L"WIC SIXEL Decoder");
    }
}

__declspec(dllexport)
STDAPI
DllRegisterServer(void) {
    wchar_t modulePath[MAX_PATH];
    wchar_t clsidKey[256];
    wchar_t inprocKey[512];
    wchar_t categoryKey[512];

    GetModuleFileNameW((HINSTANCE)&__ImageBase, modulePath, MAX_PATH);
    wsprintfW(clsidKey, L"CLSID\\%s", CLSID_SixelDecoder_String);
    RegisterStringValue(HKEY_CLASSES_ROOT, clsidKey, NULL, L"WIC SIXEL Decoder");
    wsprintfW(inprocKey, L"%s\\InprocServer32", clsidKey);
    RegisterStringValue(HKEY_CLASSES_ROOT, inprocKey, NULL, modulePath);
    RegisterStringValue(HKEY_CLASSES_ROOT, inprocKey, L"ThreadingModel", L"Both");
    wsprintfW(categoryKey,
              L"CLSID\\%s\\Instance\\%s",
              CATID_WICBitmapDecoders_String,
              CLSID_SixelDecoder_String);
    RegisterStringValue(HKEY_CLASSES_ROOT, categoryKey, L"CLSID", CLSID_SixelDecoder_String);
    RegisterStringValue(HKEY_CLASSES_ROOT, categoryKey, L"FriendlyName", L"WIC SIXEL Decoder");

    RegisterCodecKeysInCLSID(CLSID_SixelDecoder_String, modulePath);

    return S_OK;
}

__declspec(dllexport)
STDAPI
DllUnregisterServer(void)
{
    wchar_t clsidKey[256];
    wchar_t inprocKey[512];
    wchar_t patternKey[512];
    wchar_t pattern0Key[512];
    wchar_t categoryKey[512];
    wchar_t formatsKey[512];
    wchar_t formats1Key[512];

    wsprintfW(clsidKey, L"CLSID\\%s", CLSID_SixelDecoder_String);
    wsprintfW(categoryKey,
              L"CLSID\\%s\\Instance\\%s",
              CATID_WICBitmapDecoders_String,
              CLSID_SixelDecoder_String);
    wsprintfW(inprocKey, L"%s\\InprocServer32",clsidKey);
    wsprintfW(patternKey, L"%s\\Patterns",clsidKey);
    wsprintfW(pattern0Key, L"%s\\0",patternKey);
    wsprintfW(formatsKey, L"%s\\Formats\\%s",clsidKey);
    wsprintfW(formats1Key, L"%s\\%s", formatsKey, GUID_WICPixelFormat32bppBGRA_String);

    RegDeleteKeyW(HKEY_CLASSES_ROOT, inprocKey);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, categoryKey);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, pattern0Key);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, patternKey);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, formats1Key);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, formatsKey);
    RegDeleteKeyW(HKEY_CLASSES_ROOT, clsidKey);

    return S_OK;
}
/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
