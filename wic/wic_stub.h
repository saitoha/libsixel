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

#ifndef LIBSIXEL_WIC_STUB_H
# define LIBSIXEL_WIC_STUB_H

# define COBJMACROS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <wincodec.h>
# if defined(HAVE_WINCODECSDK_H)
#  include <wincodecsdk.h>
# endif
# include <unknwn.h>
# include <objbase.h>

/* custom malloc */
void * wic_malloc(size_t size);
/* custom realloc */
void * wic_realloc(void* ptr, size_t new_size);
/* custom calloc */
void * wic_calloc(size_t count, size_t size);
/* custom free */
void wic_free(void* ptr);

#ifndef IID_IWICBitmapSourceTransform
/* {3B16811B-6A43-4EC9-A813-3D930C13B940} */
static const IID IID_IWICBitmapSourceTransform = {
    0x3b16811b,
    0x6a43,
    0x4ec9,
    { 0xa8, 0x13, 0x3d, 0x93, 0x0c, 0x13, 0xb9, 0x40 }
};
#endif

#ifndef __IWICBitmapSourceTransform_INTERFACE_DEFINED__
#define __IWICBitmapSourceTransform_INTERFACE_DEFINED__

typedef struct IWICBitmapSourceTransform IWICBitmapSourceTransform;

/* vtbl */
typedef struct IWICBitmapSourceTransformVtbl {
    BEGIN_INTERFACE

    /* IUnknown */
    HRESULT (STDMETHODCALLTYPE *QueryInterface)(
        IWICBitmapSourceTransform* This, REFIID riid, void** ppvObject);
    ULONG (STDMETHODCALLTYPE *AddRef)(IWICBitmapSourceTransform* This);
    ULONG (STDMETHODCALLTYPE *Release)(IWICBitmapSourceTransform* This);

    /* IWICBitmapSourceTransform */
    HRESULT (STDMETHODCALLTYPE *CopyPixels)(
        IWICBitmapSourceTransform* This,
        /* optional */ const WICRect* prc,
        UINT uiWidth,
        UINT uiHeight,
        WICPixelFormatGUID *pguidDstFormat,
        WICBitmapTransformOptions dstTransform,
        UINT cbStride,
        UINT cbBufferSize,
        /* out */ BYTE* pbBuffer);

    HRESULT (STDMETHODCALLTYPE *GetClosestSize)(
        IWICBitmapSourceTransform* This,
        /* inout */ UINT* puiWidth,
        /* inout */ UINT* puiHeight);

    HRESULT (STDMETHODCALLTYPE *GetClosestPixelFormat)(
        IWICBitmapSourceTransform* This,
        /* inout */ WICPixelFormatGUID* pDstFormat);

    HRESULT (STDMETHODCALLTYPE *DoesSupportTransform)(
        IWICBitmapSourceTransform* This,
        WICBitmapTransformOptions dstTransform,
        /* out */ BOOL* pfIsSupported);

    END_INTERFACE
} IWICBitmapSourceTransformVtbl;

struct IWICBitmapSourceTransform {
    CONST_VTBL struct IWICBitmapSourceTransformVtbl *lpVtbl;
};

# endif /* __IWICBitmapSourceTransform_INTERFACE_DEFINED__ */
#endif /* LIBSIXEL_WIC_STUB_H */
