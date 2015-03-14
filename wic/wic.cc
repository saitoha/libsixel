
#define _CRT_SECURE_NO_WARNINGS
#include <wincodec.h>
#include <stdio.h>
#include <vector>

static const CLSID CLSID_SixelDecoder = {
    /* 15b9b4da-b155-4977-8571-cf005884bcb9 */
    0x15b9b4da,
    0xb155,
    0x4977,
    {0x85, 0x71, 0xcf, 0x00, 0x58, 0x84, 0xbc, 0xb9}
};

#define ARGB(a, r, g, b) ((WICColor)((a) << 24 | (r) << 16 | (g) << 8 | (b)))

static WICColor colortable[] = {
    ARGB(255, 0x00, 0x00, 0x00), //  0 Black
    ARGB(255, 0x14, 0x14, 0x14), //  1 Blue
    ARGB(255, 0x50, 0x0d, 0x0d), //  2 Red
    ARGB(255, 0x14, 0x50, 0x14), //  3 Green
    ARGB(255, 0x50, 0x14, 0x50), //  4 Magenta
    ARGB(255, 0x14, 0x50, 0x14), //  5 Cyan
    ARGB(255, 0x50, 0x50, 0x14), //  6 Yellow
    ARGB(255, 0x35, 0x35, 0x35), //  7 Gray 50%
    ARGB(255, 0x1a, 0x1a, 0x1a), //  8 Gray 25%
    ARGB(255, 0x21, 0x21, 0x3c), //  9 Blue*
    ARGB(255, 0x3c, 0x1a, 0x1a), // 10 Red*
    ARGB(255, 0x21, 0x3c, 0x21), // 11 Green*
    ARGB(255, 0x3c, 0x21, 0x3c), // 12 Magenta*
    ARGB(255, 0x21, 0x3c, 0x3c), // 13 Cyan*
    ARGB(255, 0x3c, 0x3c, 0x21), // 14 Yellow*
    ARGB(255, 0x50, 0x50, 0x50), // 15 Gray 75%
    ARGB(255, 0x00, 0x00, 0x00),
    ARGB(255, 0x00, 0x00, 0x5f),
    ARGB(255, 0x00, 0x00, 0x87),
    ARGB(255, 0x00, 0x00, 0xaf),  /* 16 -19  */ 
    ARGB(255, 0x00, 0x00, 0xd7),
    ARGB(255, 0x00, 0x00, 0xff),
    ARGB(255, 0x00, 0x5f, 0x00),
    ARGB(255, 0x00, 0x5f, 0x5f),  /* 20 -23  */ 
    ARGB(255, 0x00, 0x5f, 0x87),
    ARGB(255, 0x00, 0x5f, 0xaf),
    ARGB(255, 0x00, 0x5f, 0xd7),
    ARGB(255, 0x00, 0x5f, 0xff),  /* 24 -27  */ 
    ARGB(255, 0x00, 0x87, 0x00),
    ARGB(255, 0x00, 0x87, 0x5f),
    ARGB(255, 0x00, 0x87, 0x87),
    ARGB(255, 0x00, 0x87, 0xaf),  /* 28 -31  */ 
    ARGB(255, 0x00, 0x87, 0xd7),
    ARGB(255, 0x00, 0x87, 0xff),
    ARGB(255, 0x00, 0xaf, 0x00),
    ARGB(255, 0x00, 0xaf, 0x5f),  /* 32 -35  */ 
    ARGB(255, 0x00, 0xaf, 0x87),
    ARGB(255, 0x00, 0xaf, 0xaf),
    ARGB(255, 0x00, 0xaf, 0xd7),
    ARGB(255, 0x00, 0xaf, 0xff),  /* 36 -39  */ 
    ARGB(255, 0x00, 0xd7, 0x00),
    ARGB(255, 0x00, 0xd7, 0x5f),
    ARGB(255, 0x00, 0xd7, 0x87),
    ARGB(255, 0x00, 0xd7, 0xaf),  /* 40 -43  */ 
    ARGB(255, 0x00, 0xd7, 0xd7),
    ARGB(255, 0x00, 0xd7, 0xff),
    ARGB(255, 0x00, 0xff, 0x00),
    ARGB(255, 0x00, 0xff, 0x5f),  /* 44 -47  */ 
    ARGB(255, 0x00, 0xff, 0x87),
    ARGB(255, 0x00, 0xff, 0xaf),
    ARGB(255, 0x00, 0xff, 0xd7),
    ARGB(255, 0x00, 0xff, 0xff),  /* 48 -51  */ 
    ARGB(255, 0x5f, 0x00, 0x00),
    ARGB(255, 0x5f, 0x00, 0x5f),
    ARGB(255, 0x5f, 0x00, 0x87),
    ARGB(255, 0x5f, 0x00, 0xaf),  /* 52 -55  */ 
    ARGB(255, 0x5f, 0x00, 0xd7),
    ARGB(255, 0x5f, 0x00, 0xff),
    ARGB(255, 0x5f, 0x5f, 0x00),
    ARGB(255, 0x5f, 0x5f, 0x5f),  /* 56 -59  */ 
    ARGB(255, 0x5f, 0x5f, 0x87),
    ARGB(255, 0x5f, 0x5f, 0xaf),
    ARGB(255, 0x5f, 0x5f, 0xd7),
    ARGB(255, 0x5f, 0x5f, 0xff),  /* 60 -63  */ 
    ARGB(255, 0x5f, 0x87, 0x00),
    ARGB(255, 0x5f, 0x87, 0x5f),
    ARGB(255, 0x5f, 0x87, 0x87),
    ARGB(255, 0x5f, 0x87, 0xaf),  /* 64 -67  */ 
    ARGB(255, 0x5f, 0x87, 0xd7),
    ARGB(255, 0x5f, 0x87, 0xff),
    ARGB(255, 0x5f, 0xaf, 0x00),
    ARGB(255, 0x5f, 0xaf, 0x5f),  /* 68 -71  */ 
    ARGB(255, 0x5f, 0xaf, 0x87),
    ARGB(255, 0x5f, 0xaf, 0xaf),
    ARGB(255, 0x5f, 0xaf, 0xd7),
    ARGB(255, 0x5f, 0xaf, 0xff),  /* 72 -75  */ 
    ARGB(255, 0x5f, 0xd7, 0x00),
    ARGB(255, 0x5f, 0xd7, 0x5f),
    ARGB(255, 0x5f, 0xd7, 0x87),
    ARGB(255, 0x5f, 0xd7, 0xaf),  /* 76 -79  */ 
    ARGB(255, 0x5f, 0xd7, 0xd7),
    ARGB(255, 0x5f, 0xd7, 0xff),
    ARGB(255, 0x5f, 0xff, 0x00),
    ARGB(255, 0x5f, 0xff, 0x5f),  /* 80 -83  */ 
    ARGB(255, 0x5f, 0xff, 0x87),
    ARGB(255, 0x5f, 0xff, 0xaf),
    ARGB(255, 0x5f, 0xff, 0xd7),
    ARGB(255, 0x5f, 0xff, 0xff),  /* 84 -87  */ 
    ARGB(255, 0x87, 0x00, 0x00),
    ARGB(255, 0x87, 0x00, 0x5f),
    ARGB(255, 0x87, 0x00, 0x87),
    ARGB(255, 0x87, 0x00, 0xaf),  /* 88 -91  */ 
    ARGB(255, 0x87, 0x00, 0xd7),
    ARGB(255, 0x87, 0x00, 0xff),
    ARGB(255, 0x87, 0x5f, 0x00),
    ARGB(255, 0x87, 0x5f, 0x5f),  /* 92 -95  */ 
    ARGB(255, 0x87, 0x5f, 0x87),
    ARGB(255, 0x87, 0x5f, 0xaf),
    ARGB(255, 0x87, 0x5f, 0xd7),
    ARGB(255, 0x87, 0x5f, 0xff),  /* 96 -99  */ 
    ARGB(255, 0x87, 0x87, 0x00),
    ARGB(255, 0x87, 0x87, 0x5f),
    ARGB(255, 0x87, 0x87, 0x87),
    ARGB(255, 0x87, 0x87, 0xaf),  /* 100-103 */ 
    ARGB(255, 0x87, 0x87, 0xd7),
    ARGB(255, 0x87, 0x87, 0xff),
    ARGB(255, 0x87, 0xaf, 0x00),
    ARGB(255, 0x87, 0xaf, 0x5f),  /* 104-107 */ 
    ARGB(255, 0x87, 0xaf, 0x87),
    ARGB(255, 0x87, 0xaf, 0xaf),
    ARGB(255, 0x87, 0xaf, 0xd7),
    ARGB(255, 0x87, 0xaf, 0xff),  /* 108-111 */ 
    ARGB(255, 0x87, 0xd7, 0x00),
    ARGB(255, 0x87, 0xd7, 0x5f),
    ARGB(255, 0x87, 0xd7, 0x87),
    ARGB(255, 0x87, 0xd7, 0xaf),  /* 112-115 */ 
    ARGB(255, 0x87, 0xd7, 0xd7),
    ARGB(255, 0x87, 0xd7, 0xff),
    ARGB(255, 0x87, 0xff, 0x00),
    ARGB(255, 0x87, 0xff, 0x5f),  /* 116-119 */ 
    ARGB(255, 0x87, 0xff, 0x87),
    ARGB(255, 0x87, 0xff, 0xaf),
    ARGB(255, 0x87, 0xff, 0xd7),
    ARGB(255, 0x87, 0xff, 0xff),  /* 120-123 */ 
    ARGB(255, 0xaf, 0x00, 0x00),
    ARGB(255, 0xaf, 0x00, 0x5f),
    ARGB(255, 0xaf, 0x00, 0x87),
    ARGB(255, 0xaf, 0x00, 0xaf),  /* 124-127 */ 
    ARGB(255, 0xaf, 0x00, 0xd7),
    ARGB(255, 0xaf, 0x00, 0xff),
    ARGB(255, 0xaf, 0x5f, 0x00),
    ARGB(255, 0xaf, 0x5f, 0x5f),  /* 128-131 */ 
    ARGB(255, 0xaf, 0x5f, 0x87),
    ARGB(255, 0xaf, 0x5f, 0xaf),
    ARGB(255, 0xaf, 0x5f, 0xd7),
    ARGB(255, 0xaf, 0x5f, 0xff),  /* 132-135 */ 
    ARGB(255, 0xaf, 0x87, 0x00),
    ARGB(255, 0xaf, 0x87, 0x5f),
    ARGB(255, 0xaf, 0x87, 0x87),
    ARGB(255, 0xaf, 0x87, 0xaf),  /* 136-139 */ 
    ARGB(255, 0xaf, 0x87, 0xd7),
    ARGB(255, 0xaf, 0x87, 0xff),
    ARGB(255, 0xaf, 0xaf, 0x00),
    ARGB(255, 0xaf, 0xaf, 0x5f),  /* 140-143 */ 
    ARGB(255, 0xaf, 0xaf, 0x87),
    ARGB(255, 0xaf, 0xaf, 0xaf),
    ARGB(255, 0xaf, 0xaf, 0xd7),
    ARGB(255, 0xaf, 0xaf, 0xff),  /* 144-147 */ 
    ARGB(255, 0xaf, 0xd7, 0x00),
    ARGB(255, 0xaf, 0xd7, 0x5f),
    ARGB(255, 0xaf, 0xd7, 0x87),
    ARGB(255, 0xaf, 0xd7, 0xaf),  /* 148-151 */ 
    ARGB(255, 0xaf, 0xd7, 0xd7),
    ARGB(255, 0xaf, 0xd7, 0xff),
    ARGB(255, 0xaf, 0xff, 0x00),
    ARGB(255, 0xaf, 0xff, 0x5f),  /* 152-155 */ 
    ARGB(255, 0xaf, 0xff, 0x87),
    ARGB(255, 0xaf, 0xff, 0xaf),
    ARGB(255, 0xaf, 0xff, 0xd7),
    ARGB(255, 0xaf, 0xff, 0xff),  /* 156-159 */ 
    ARGB(255, 0xd7, 0x00, 0x00),
    ARGB(255, 0xd7, 0x00, 0x5f),
    ARGB(255, 0xd7, 0x00, 0x87),
    ARGB(255, 0xd7, 0x00, 0xaf),  /* 160-163 */ 
    ARGB(255, 0xd7, 0x00, 0xd7),
    ARGB(255, 0xd7, 0x00, 0xff),
    ARGB(255, 0xd7, 0x5f, 0x00),
    ARGB(255, 0xd7, 0x5f, 0x5f),  /* 164-167 */ 
    ARGB(255, 0xd7, 0x5f, 0x87),
    ARGB(255, 0xd7, 0x5f, 0xaf),
    ARGB(255, 0xd7, 0x5f, 0xd7),
    ARGB(255, 0xd7, 0x5f, 0xff),  /* 168-171 */ 
    ARGB(255, 0xd7, 0x87, 0x00),
    ARGB(255, 0xd7, 0x87, 0x5f),
    ARGB(255, 0xd7, 0x87, 0x87),
    ARGB(255, 0xd7, 0x87, 0xaf),  /* 172-175 */ 
    ARGB(255, 0xd7, 0x87, 0xd7),
    ARGB(255, 0xd7, 0x87, 0xff),
    ARGB(255, 0xd7, 0xaf, 0x00),
    ARGB(255, 0xd7, 0xaf, 0x5f),  /* 176-179 */ 
    ARGB(255, 0xd7, 0xaf, 0x87),
    ARGB(255, 0xd7, 0xaf, 0xaf),
    ARGB(255, 0xd7, 0xaf, 0xd7),
    ARGB(255, 0xd7, 0xaf, 0xff),  /* 180-183 */ 
    ARGB(255, 0xd7, 0xd7, 0x00),
    ARGB(255, 0xd7, 0xd7, 0x5f),
    ARGB(255, 0xd7, 0xd7, 0x87),
    ARGB(255, 0xd7, 0xd7, 0xaf),  /* 184-187 */ 
    ARGB(255, 0xd7, 0xd7, 0xd7),
    ARGB(255, 0xd7, 0xd7, 0xff),
    ARGB(255, 0xd7, 0xff, 0x00),
    ARGB(255, 0xd7, 0xff, 0x5f),  /* 188-191 */ 
    ARGB(255, 0xd7, 0xff, 0x87),
    ARGB(255, 0xd7, 0xff, 0xaf),
    ARGB(255, 0xd7, 0xff, 0xd7),
    ARGB(255, 0xd7, 0xff, 0xff),  /* 192-195 */ 
    ARGB(255, 0xff, 0x00, 0x00),
    ARGB(255, 0xff, 0x00, 0x5f),
    ARGB(255, 0xff, 0x00, 0x87),
    ARGB(255, 0xff, 0x00, 0xaf),  /* 196-199 */ 
    ARGB(255, 0xff, 0x00, 0xd7),
    ARGB(255, 0xff, 0x00, 0xff),
    ARGB(255, 0xff, 0x5f, 0x00),
    ARGB(255, 0xff, 0x5f, 0x5f),  /* 200-203 */ 
    ARGB(255, 0xff, 0x5f, 0x87),
    ARGB(255, 0xff, 0x5f, 0xaf),
    ARGB(255, 0xff, 0x5f, 0xd7),
    ARGB(255, 0xff, 0x5f, 0xff),  /* 204-207 */ 
    ARGB(255, 0xff, 0x87, 0x00),
    ARGB(255, 0xff, 0x87, 0x5f),
    ARGB(255, 0xff, 0x87, 0x87),
    ARGB(255, 0xff, 0x87, 0xaf),  /* 208-211 */ 
    ARGB(255, 0xff, 0x87, 0xd7),
    ARGB(255, 0xff, 0x87, 0xff),
    ARGB(255, 0xff, 0xaf, 0x00),
    ARGB(255, 0xff, 0xaf, 0x5f),  /* 212-215 */ 
    ARGB(255, 0xff, 0xaf, 0x87),
    ARGB(255, 0xff, 0xaf, 0xaf),
    ARGB(255, 0xff, 0xaf, 0xd7),
    ARGB(255, 0xff, 0xaf, 0xff),  /* 216-219 */ 
    ARGB(255, 0xff, 0xd7, 0x00),
    ARGB(255, 0xff, 0xd7, 0x5f),
    ARGB(255, 0xff, 0xd7, 0x87),
    ARGB(255, 0xff, 0xd7, 0xaf),  /* 220-223 */ 
    ARGB(255, 0xff, 0xd7, 0xd7),
    ARGB(255, 0xff, 0xd7, 0xff),
    ARGB(255, 0xff, 0xff, 0x00),
    ARGB(255, 0xff, 0xff, 0x5f),  /* 224-227 */ 
    ARGB(255, 0xff, 0xff, 0x87),
    ARGB(255, 0xff, 0xff, 0xaf),
    ARGB(255, 0xff, 0xff, 0xd7),
    ARGB(255, 0xff, 0xff, 0xff),  /* 228-231 */ 
    ARGB(255, 0x08, 0x08, 0x08),
    ARGB(255, 0x12, 0x12, 0x12),
    ARGB(255, 0x1c, 0x1c, 0x1c),
    ARGB(255, 0x26, 0x26, 0x26),  /* 232-235 */ 
    ARGB(255, 0x30, 0x30, 0x30),
    ARGB(255, 0x3a, 0x3a, 0x3a),
    ARGB(255, 0x44, 0x44, 0x44),
    ARGB(255, 0x4e, 0x4e, 0x4e),  /* 236-239 */ 
    ARGB(255, 0x58, 0x58, 0x58),
    ARGB(255, 0x62, 0x62, 0x62),
    ARGB(255, 0x6c, 0x6c, 0x6c),
    ARGB(255, 0x76, 0x76, 0x76),  /* 240-243 */ 
    ARGB(255, 0x80, 0x80, 0x80),
    ARGB(255, 0x8a, 0x8a, 0x8a),
    ARGB(255, 0x94, 0x94, 0x94),
    ARGB(255, 0x9e, 0x9e, 0x9e),  /* 244-247 */ 
    ARGB(255, 0xa8, 0xa8, 0xa8),
    ARGB(255, 0xb2, 0xb2, 0xb2),
    ARGB(255, 0xbc, 0xbc, 0xbc),
    ARGB(255, 0xc6, 0xc6, 0xc6),  /* 248-251 */ 
    ARGB(255, 0xd0, 0xd0, 0xd0),
    ARGB(255, 0xda, 0xda, 0xda),
    ARGB(255, 0xe4, 0xe4, 0xe4),
    ARGB(255, 0xee, 0xee, 0xee)   /* 252-255 */ 
};

class UnknownImpl
{
private:
    int numReferences;

public:
    UnknownImpl()
        : numReferences(0)
    {
    }

    ULONG STDMETHODCALLTYPE AddRef()
    {
        return ++numReferences;
    }

    ULONG STDMETHODCALLTYPE Release()
    {
        ULONG result;

        if (numReferences > 0)
        {
            --numReferences;
            result = numReferences;
        }
        else
        {
            result = numReferences = 0;
        }

        return result;
    }
};

class SixelFrameDecode : public IWICBitmapFrameDecode
{
public:
    SixelFrameDecode(IWICImagingFactory *pIFactory, UINT num)
        : factory(pIFactory)
        , bitmapSource(NULL)
        , palette(NULL)
        , thumbnail(NULL)
        , preview(NULL)
    {
        if (NULL != factory)
        {
            factory->AddRef();
        }
    }

    virtual ~SixelFrameDecode()
    {
    }

    // IUnknown Interface
    STDMETHOD(QueryInterface)(REFIID iid, void **ppvObject)
    {
        HRESULT result = E_INVALIDARG;

        if (ppvObject)
        {
            if (iid == IID_IUnknown)
            {
                *ppvObject = static_cast<IUnknown*>(this);
                AddRef();

                result = S_OK;
            }
            else if (iid == IID_IWICBitmapFrameDecode)
            {
                *ppvObject = static_cast<IWICBitmapFrameDecode*>(this);
                AddRef();

                result = S_OK;
            }
            else if (iid == IID_IWICBitmapSource)
            {
                    ::MessageBox(0, "Jl", 0, 0);
                *ppvObject = bitmapSource;

                if (NULL != bitmapSource)
                {
                    bitmapSource->AddRef();
                }

                result = S_OK;
            }
            else
            {
                result = E_NOINTERFACE;
            }
        }

        return result;
    }

    STDMETHOD_(ULONG, AddRef)()
    {
        return unknownImpl.AddRef();
    }

    STDMETHOD_(ULONG, Release)()
    {
        ULONG result = unknownImpl.Release();
        if (0 == result)
        {
            delete this;
        }
        return result;
    }


    // IWICBitmapFrameDecode Interface
    STDMETHOD(GetMetadataQueryReader)(
        /* [out] */ IWICMetadataQueryReader **ppIMetadataQueryReader)
    {
//        ::MessageBox(NULL, "GetMetadataQueryReader", NULL, NULL);
        return WINCODEC_ERR_UNSUPPORTEDOPERATION;
    }


    STDMETHOD(GetColorContexts)(
        /* [in] */ UINT cCount,
        /* [in] [out] */ IWICColorContext **ppIColorContexts,
        /* [out] */ UINT *pcActualCount)
    {
        HRESULT result = S_OK;
        if (ppIColorContexts == NULL)
        {
            //return the number of color contexts
            if (pcActualCount != NULL)
            {
                *pcActualCount = 1;
            }
            else
            {
                result = E_INVALIDARG;
            }
        }
        return result;
    }

    STDMETHOD(GetThumbnail)(
        /* [out] */ IWICBitmapSource **ppIThumbnail)
    {
        ::MessageBox(NULL, "GetThumbnail", NULL, NULL);
        HRESULT result = S_OK;

        if (NULL == ppIThumbnail)
        {
            result = E_INVALIDARG;
        }

        if (SUCCEEDED(result))
        {
            *ppIThumbnail = thumbnail;

            if (NULL != thumbnail)
            {
                thumbnail->AddRef();
            }
            else
            {
                result = WINCODEC_ERR_CODECNOTHUMBNAIL;
            }
        }

        return result;
    }

    // IWICBitmapSource Interface
    STDMETHOD(GetSize)(
        /* [out] */ UINT *puiWidth,
        /* [out] */ UINT *puiHeight)
    {
        HRESULT result = E_UNEXPECTED;

        if (bitmapSource)
        {
            result = bitmapSource->GetSize(puiWidth, puiHeight);
        }
        else
        {
            *puiWidth = 100;
            *puiHeight = 100;
            result = S_OK;
        }

        return result;
    }

    STDMETHOD(GetPixelFormat)(
        /* [out] */ WICPixelFormatGUID *pPixelFormat)
    {
        HRESULT result = E_UNEXPECTED;
        if (bitmapSource)
        {
            result = bitmapSource->GetPixelFormat(pPixelFormat);
        }
        else
        {
            *pPixelFormat = GUID_WICPixelFormat24bppRGB;
            *pPixelFormat = GUID_WICPixelFormat8bppIndexed;
            result = S_OK;
        }
        return result;
    }

    STDMETHOD(GetResolution)(
        /* [out] */ double *pDpiX,
        /* [out] */ double *pDpiY)
    {
        HRESULT result = E_UNEXPECTED;

        if (bitmapSource)
        {
            result = bitmapSource->GetResolution(pDpiX, pDpiY);
        }
        else
        {
            *pDpiX = 75;
            *pDpiY = 75;
            result = S_OK;
        }
        return result;
    }

    STDMETHOD(CopyPalette)(/* [in] */ IWICPalette *pIPalette)
    {
        HRESULT result = S_OK;
        UINT numColors = 0x100;

        if (NULL == pIPalette)
        {
            result = E_INVALIDARG;
        }

        if (SUCCEEDED(result))
        {
            result = pIPalette->InitializeCustom(colortable, numColors);
        }

//        if (SUCCEEDED(result))
//        ::MessageBox(NULL, "CopyPalette - 1", NULL, NULL);

//        if (SUCCEEDED(result))
//        {
//            if (NULL != palette)
//            {
//                pIPalette->InitializeFromPalette(palette);
//            }
//            else
//            {
//                result = E_UNEXPECTED;
//            }
//        }

        return result;
    }

    STDMETHOD(CopyPixels)(
        /* [in] */ const WICRect *prc,
        /* [in] */ UINT cbStride,
        /* [in] */ UINT cbPixelsSize,
        /* [out] */ BYTE *pbPixels
    )
    {

        /*
        typedef struct WICRect {
          INT X;
          INT Y;
          INT Width;
          INT Height;
        } WICRect;
        */
        char buf[256];
        sprintf(buf,
                "CopyPixels[%d][%d][%d][%d],stride=[%d],pixelsize=[%d]",
                prc->X, prc->Y, prc->Width, prc->Height,
                cbStride, cbPixelsSize);
        //::MessageBox(NULL, buf, NULL, NULL);
        HRESULT result = E_UNEXPECTED;

        if (bitmapSource)
        {
            result = bitmapSource->CopyPixels(prc, cbStride, cbPixelsSize, pbPixels);
        } else {
            for (size_t i = 0; i < cbStride; ++i) {
                pbPixels[i] = i % 256;
            }
        }
        result = S_OK;
        return result;
    }


public:
    IWICImagingFactory *factory;
    IWICBitmapSource *bitmapSource;
    IWICPalette *palette;
    IWICBitmapSource *thumbnail;
    IWICBitmapSource *preview;

private:
    void ReleaseMembers()
    {
        if (factory)
        {
            factory->Release();
            factory = NULL;
        }
        if (bitmapSource)
        {
            bitmapSource->Release();
            bitmapSource = NULL;
        }
        if (palette)
        {
            palette->Release();
            palette = NULL;
        }
        if (thumbnail)
        {
            thumbnail->Release();
            thumbnail = NULL;
        }
        if (preview)
        {
            preview->Release();
            preview = NULL;
        }
    }

    UnknownImpl unknownImpl;
};

// IWICBitmapFrameDecode Interface

static HRESULT InputBitmapSource(IStream *stream, IWICImagingFactory *factory, IWICBitmapSource **bitmapSource)
{
    HRESULT result = S_OK;

    IWICBitmap *bitmap = NULL;
    IWICBitmapLock *bitmapLock = NULL;

    UINT width = 0, height = 0;
    double dpiX = 0, dpiY = 0;
    WICPixelFormatGUID pixelFormat = { 0 };
    UINT srcStride = 0;
    UINT destStride = 0;
    BYTE *data = NULL;
    UINT cbBufferSize = 0;

    if ((NULL == stream) || (NULL == factory) || (NULL == bitmapSource))
    {
        result = E_INVALIDARG;
    }

    if (SUCCEEDED(result))
    {
        //InputValue(stream, width);
        //InputValue(stream, height);
        //InputValue(stream, dpiX);
        //InputValue(stream, dpiY);
        //InputValue(stream, srcStride);
        //result = InputValue(stream, pixelFormat);
    }

    // Create the bitmap
    if (SUCCEEDED(result))
    {
        result = factory->CreateBitmap(width, height,
                                       pixelFormat,
                                       WICBitmapCacheOnLoad,
                                       &bitmap);
    }

    // Set the resolution
    if (SUCCEEDED(result))
    {
        result = bitmap->SetResolution(dpiX, dpiY);
    }

    // Lock it so that we can store the data
    if (SUCCEEDED(result))
    {
        WICRect rct;
        rct.X = 0;
        rct.Y = 0;
        rct.Width = width;
        rct.Height = height;

        result = bitmap->Lock(&rct, WICBitmapLockWrite, &bitmapLock);
    }

    if (SUCCEEDED(result))
    {
        result = bitmapLock->GetDataPointer(&cbBufferSize, &data);
    }

    if (SUCCEEDED(result))
    {
        result = bitmapLock->GetStride(&destStride);
    }

    // Read the data from the stream
    if (SUCCEEDED(result))
    {
        // We must read one scanline at a time because the input stride
        // may not equal the output stride
        for (UINT y = 0; y < height; y++)
        {
            //InputValues(stream, data, srcStride);
            // Prepare for the next scanline
            data += destStride;
        }
    }

    // Close the lock
    if (bitmapLock && bitmap)
    {
        if(bitmapLock)
        {
            bitmapLock->Release();
            bitmapLock = NULL;
        }
    }

    // Finish
    if (SUCCEEDED(result))
    {
        result = bitmap->QueryInterface(IID_IWICBitmapSource, (void**)bitmapSource);
        if (SUCCEEDED(result))
        {
            bitmap->Release();
        }
    }
    else
    {
        if (bitmap)
        {
            bitmap->Release();
        }
        *bitmapSource = NULL;
    }

    return result;
}

static HRESULT InputBitmapPalette(IStream *stream, IWICImagingFactory *factory, IWICPalette **palette)
{
    HRESULT result = S_OK;

    WICColor *colors = NULL;

    UINT numColors = 0;

    if ((NULL == stream) || (NULL == factory) || (NULL == palette))
    {
        result = E_INVALIDARG;
    }

    // Create the palette
    if (SUCCEEDED(result))
    {
        result = factory->CreatePalette(palette);
    }

    // Read the colors
    if (SUCCEEDED(result))
    {
//        result = InputValue(stream, numColors);
    }

    if (SUCCEEDED(result))
    {
        colors = new WICColor[numColors];

        if (NULL == colors)
        {
            result = E_OUTOFMEMORY;
        }
    }

    if (SUCCEEDED(result))
    {
//        result = InputValues(stream, colors, numColors);
    }

    if (SUCCEEDED(result))
    {
        result = (*palette)->InitializeCustom(colors, numColors);
    }

    return result;
}


class SixelDecoder : public IWICBitmapDecoder
{
public:
    SixelDecoder()
        : factory(NULL) , palette(NULL) , thumbnail(NULL)
        , preview(NULL) , p_frame(NULL)
    {
    }

    virtual ~SixelDecoder()
    {
        ReleaseMembers(true);
    }

    // IUnknown Interface
    STDMETHOD(QueryInterface)(REFIID iid, void **ppvObject)
    {
        HRESULT result = E_INVALIDARG;

        if (ppvObject)
        {
            if (iid == IID_IUnknown)
            {
                *ppvObject = static_cast<IUnknown*>(this);
                AddRef();

                result = S_OK;
            }
            else if (iid == IID_IWICBitmapDecoder)
            {
                *ppvObject = static_cast<IWICBitmapDecoder*>(this);
                AddRef();

                result = S_OK;
            }
            else
            {
                result = E_NOINTERFACE;
            }
        }
        return result;
    }

    STDMETHOD_(ULONG, AddRef)()
    {
        return unknownImpl.AddRef();
    }

    STDMETHOD_(ULONG, Release)()
    {
        ULONG result = unknownImpl.Release();
        if (0 == result)
        {
            delete this;
        }
        return result;
    }

    // IWICBitmapDecoder Interface
    STDMETHOD(QueryCapability)(
        /* [in] */ IStream *pIStream,
        /* [out] */ DWORD *pCapability)
    {
        HRESULT result = S_OK;
        LARGE_INTEGER zero = { 0 };
        ULARGE_INTEGER curPos = { 0 };

        ::MessageBox(NULL, "SixelDecoder::QueryCapability", NULL, NULL);
        if ((NULL == pIStream) || (NULL == pCapability))
        {
            result = E_INVALIDARG;
        }
        if (SUCCEEDED(result))
        {
            result = pIStream->Seek(zero, STREAM_SEEK_CUR, &curPos);
        }
        if (SUCCEEDED(result))
        {
            *pCapability = WICBitmapDecoderCapabilityCanDecodeAllImages ||
                           WICBitmapDecoderCapabilityCanDecodeThumbnail;
            *pCapability = WICBitmapDecoderCapabilityCanDecodeAllImages ||
                           WICBitmapDecoderCapabilityCanDecodeThumbnail ||
                           WICBitmapDecoderCapabilityCanEnumerateMetadata ||
                           WICBitmapDecoderCapabilitySameEncoder;
        }
        return result;
    }

    STDMETHOD(Initialize)(
        /* [in] */ IStream *pIStream,
        /* [in] */ WICDecodeOptions cacheOptions)
    {
//        ::MessageBox(NULL, "Initialize", NULL, NULL);
        HRESULT result = S_OK;
        ULONG numRead = 0;
        UINT cbBufferSize = 0;

        ReleaseMembers(true);

        if (pIStream == NULL) {
            return E_INVALIDARG;
        }

        UINT width = 1024;
        UINT height = 1024;
        double dpiX = 75;
        double dpiY = 75;
        UINT destStride = 0;
        BYTE *data = NULL;
        IWICBitmap *bitmap = NULL;
        IWICBitmapLock *bitmapLock = NULL;
        IWICBitmapSource **bitmapSource;

        WICPixelFormatGUID pixelFormat = { 0 };
//        pixelFormat = GUID_WICPixelFormat32bppBGRA;
        pixelFormat = GUID_WICPixelFormat32bppPBGRA;
        //pixelFormat = GUID_WICPixelFormat8bppIndexed;

        VerifyFactory();

        if (!SUCCEEDED(result))
        {
            return result;
        }

        result = factory->CreateBitmap(width, height,
                                       pixelFormat,
                                       WICBitmapCacheOnLoad, &bitmap);

        if (!SUCCEEDED(result))
        {
            return result;
        }

        result = bitmap->SetResolution(dpiX, dpiY);

        // Set the resolution
        if (!SUCCEEDED(result))
        {
            bitmap->Release();
            return result;
        }

        // Lock it so that we can store the data
        WICRect rect;
        rect.X = 0;
        rect.Y = 0;
        rect.Width = width;
        rect.Height = height;
        result = bitmap->Lock(&rect, WICBitmapLockWrite, &bitmapLock);

        if (!SUCCEEDED(result))
        {
            bitmap->Release();
            return result;
        }

        result = bitmapLock->GetDataPointer(&cbBufferSize, &data);

        if (!SUCCEEDED(result))
        {
            bitmap->Release();
            bitmapLock->Release();
            return result;
        }

        result = bitmapLock->GetStride(&destStride);

        if (!SUCCEEDED(result))
        {
            bitmap->Release();
            bitmapLock->Release();
            return result;
        }

        // We must read one scanline at a time because the input stride
        // may not equal the output stride
        for (UINT y = 0; y < height; y++)
        {
            for (UINT x = 0; x < width; ++x) {
                data[x * 4 + 0] = x % 256;
                data[x * 4 + 1] = x % 256;
                data[x * 4 + 2] = x % 256;
                data[x * 4 + 3] = 0xff;
            }
            // Prepare for the next scanline
            data += destStride;
        }

        // Read the data from the stream
        if (!SUCCEEDED(result))
        {
            return result;
        }

        // Close the lock
        bitmapLock->Release();
        bitmap->Release();

        CHAR buffer[1024];
        CHAR c;
        enum STATE {
            STATE_GROUND = 0,
            STATE_ESC,
            STATE_DCS,
            STATE_DCS_ESC,
            STATE_OSC,
            STATE_OSC_ESC,
        };
        int state;
        int dcs_state;
        int i;
        while (result == S_OK) {
            result = pIStream->Read(&buffer, sizeof(buffer), &numRead);
            if (numRead <= 0) {
                break;
            }

            //char buf2[16];
            //sprintf(buf2, "[%d]", (int)numRead);
            //::MessageBox(NULL, buf2, NULL, NULL);
            for (i = 0; i < numRead; ++i)
            {
                c = buffer[c];
                switch (state) {
                case STATE_GROUND:
                    if (c == '\033') {
                        state = STATE_ESC;
                    }
                    break;
                case STATE_ESC:
                    if (c == 'P') {
                        state = STATE_DCS;
                    }
                    break;
                case STATE_DCS:
                    if (c == '\033') {
                        state = STATE_DCS_ESC;
                    } else if (c == '\\') {
                        state = STATE_GROUND;
                    } else if (c >= 0x20 && c < 0x7f) {
                        
                    }
                    break;
                case STATE_DCS_ESC:
                    if (c == '\033') {
                        state = STATE_DCS_ESC;
                    } else {
                        state = STATE_DCS;
                    }
                    break;
                case STATE_OSC:
                    break;
                case STATE_OSC_ESC:
                    break;
                default:
                    break;
                }
            }

        }

        if (SUCCEEDED(result))
        {
            result = VerifyFactory();
        }

        p_frame = new SixelFrameDecode(factory, 0);
        p_frame->AddRef();
        return result;
    }

    STDMETHOD(GetContainerFormat)(/* [out] */ GUID *pguidContainerFormat)
    {
        HRESULT result = E_INVALIDARG;

        if (NULL != pguidContainerFormat)
        {
            result = S_OK;
            *pguidContainerFormat = CLSID_SixelDecoder;
//        ::MessageBox(NULL, "GetContainerFormat", NULL, NULL);
        }

        return result;
    }

    STDMETHOD(GetDecoderInfo)(/* [out] */ IWICBitmapDecoderInfo **ppIDecoderInfo)
    {
        ::MessageBox(NULL, "GetDecoderInfo", NULL, NULL);
        HRESULT result = S_OK;

        IWICComponentInfo *compInfo = NULL;

        if (SUCCEEDED(result))
        {
            result = VerifyFactory();
        }

        if (SUCCEEDED(result))
        {
            result = factory->CreateComponentInfo(CLSID_SixelDecoder, &compInfo);
        }

        if (SUCCEEDED(result))
        {
            result = compInfo->QueryInterface(IID_IWICBitmapDecoderInfo, (void**)ppIDecoderInfo);
        }

        if (compInfo)
        {
            compInfo->Release();
        }

        return result;
    }

    STDMETHOD(CopyPalette)(
        /* [in] */ IWICPalette *pIPalette)
    {
        ::MessageBox(NULL, "CopyPalette", NULL, NULL);
        HRESULT result = S_OK;
        WICColor *colors;
        UINT numColors = 256;

        if (NULL == pIPalette)
        {
            result = E_INVALIDARG;
        }

//        // Create the palette
//        if (SUCCEEDED(result))
//        {
//            result = factory->CreatePalette(palette);
//        }
    
        // Read the colors
        if (SUCCEEDED(result))
        {
            colors = new WICColor[numColors];
            if (NULL == colors)
            {
                result = E_OUTOFMEMORY;
            }
        }

        if (SUCCEEDED(result))
        {
            for (size_t i = 0; i < numColors; ++i) {
                colors[i] = i << 16 | (numColors - i) << 8 | i;
            }
        }

        if (SUCCEEDED(result))
        {
            result = pIPalette->InitializeCustom(colors, numColors);
        }

//        if (SUCCEEDED(result))
//        {
//            if (NULL != palette)
//            {
//                pIPalette->InitializeFromPalette(palette);
//            }
//            else
//            {
//                result = E_UNEXPECTED;
//            }
//        }

        return result;
    }

    STDMETHOD(GetMetadataQueryReader)(
        /* [out] */ IWICMetadataQueryReader **ppIMetadataQueryReader)
    {
//        ::MessageBox(NULL, "GetMetadataQueryReader", NULL, NULL);
        return WINCODEC_ERR_UNSUPPORTEDOPERATION;
        return E_NOTIMPL;
    }

    STDMETHOD(GetPreview)(
        /* [out] */ IWICBitmapSource **ppIPreview)
    {
        ::MessageBox(NULL, "GetPreview", NULL, NULL);
        HRESULT result = S_OK;

        if (NULL == ppIPreview)
        {
            result = E_INVALIDARG;
        }

        if (SUCCEEDED(result))
        {
            if (NULL != preview)
            {
                result = preview->QueryInterface(IID_IWICBitmapSource,
                                                 (void**)ppIPreview);
            }
            else
            {
                result = E_UNEXPECTED;
            }
        }

        return result;
    }

    STDMETHOD(GetColorContexts)(
        /* [in] */ UINT cCount,
        /* [in] [out] */ IWICColorContext **ppIColorContexts,
        /* [out] */ UINT *pcActualCount)
    {
        HRESULT result = S_OK;

        if (ppIColorContexts == NULL)
        {
            //return the number of color contexts
            if (pcActualCount != NULL)
            {
                *pcActualCount = 1;
            }
            else
            {
                result = E_INVALIDARG;
            }
        }
        return result;
    }

    STDMETHOD(GetThumbnail)(
        /* [out] */ IWICBitmapSource **ppIThumbnail)
    {
        ::MessageBox(NULL, "GetThumbnail", NULL, NULL);
        HRESULT result = S_OK;

        if (NULL == ppIThumbnail)
        {
            result = E_INVALIDARG;
        }

        if (SUCCEEDED(result))
        {
            if (NULL != thumbnail)
            {
                result = thumbnail->QueryInterface(IID_IWICBitmapSource,
                                                   (void**)ppIThumbnail);
            }
            else
            {
                result = WINCODEC_ERR_CODECNOTHUMBNAIL;
            }
        }

        return result;
    }

    STDMETHOD(GetFrameCount)(
        /* [out] */ UINT *pCount)
    {
        HRESULT result = S_OK;

        if (NULL == pCount)
        {
            result = E_INVALIDARG;
        }

        if (SUCCEEDED(result))
        {
            *pCount = 1;
        }

        return result;
    }

    STDMETHOD(GetFrame)(
        /* [in] */ UINT index,
        /* [out] */ IWICBitmapFrameDecode **ppIBitmapFrame)
    {
        char buf[15];
        sprintf(buf, "GetFrame[%d]", index);
        HRESULT result = S_OK;

        if (index >= 1 || NULL == ppIBitmapFrame)
        {
            result = E_INVALIDARG;
        }

        if (SUCCEEDED(result))
        {
            result = p_frame->QueryInterface(IID_IWICBitmapFrameDecode,
                                             (void**)ppIBitmapFrame);
        }
//        ::MessageBox(NULL, buf, NULL, NULL);

        return result;
    }

protected :
    HRESULT VerifyFactory()
    {
        if (NULL == factory)
        {
            return CoCreateInstance(CLSID_WICImagingFactory,
                                    NULL,
                                    CLSCTX_INPROC_SERVER,
                                    IID_IWICImagingFactory,
                                    (LPVOID*) &factory);
        }
        else
        {
            return S_OK;
        }
    }

    void ReleaseMembers(bool releaseFactory)
    {
        if (releaseFactory && factory)
        {
            factory->Release();
            factory = NULL;
        }
        if (p_frame)
        {
            p_frame->Release();
            p_frame = NULL;
        }
        if (palette)
        {
            palette->Release();
            palette = NULL;
        }
        if (thumbnail)
        {
            thumbnail->Release();
            thumbnail = NULL;
        }
        if (preview)
        {
            preview->Release();
            preview = NULL;
        }
    }

    IWICImagingFactory *factory;
    SixelFrameDecode* p_frame;
    IWICPalette *palette;
    IWICBitmapSource *thumbnail;
    IWICBitmapSource *preview;

private:
    UnknownImpl unknownImpl;
};


class SixelCodecsRegistryManager
{
public:
    HRESULT Register()
    {
        return S_OK;
    }

    HRESULT Unregister()
    {
        HRESULT result = S_OK;
        return result;
    }
};
SixelCodecsRegistryManager register_manager;

template< typename T >
class SixelCodecClassFactory : public IClassFactory
{
public:
    SixelCodecClassFactory() {}

    virtual ~SixelCodecClassFactory() {}

    // IUnknown Interface
    STDMETHOD(QueryInterface)(REFIID riid, void **ppv)
    {
        HRESULT result = E_INVALIDARG;

        if (ppv)
        {
            if (riid == IID_IUnknown)
            {
                *ppv = static_cast<IUnknown*>(this);
                AddRef();

                result = S_OK;
            }
            else if (riid == IID_IClassFactory)
            {
                *ppv = static_cast<IClassFactory*>(this);
                AddRef();

                result = S_OK;
            }
            else
            {
                result = E_NOINTERFACE;
            }
        }

        return result;
    }

    STDMETHOD_(ULONG, AddRef)()
    {
        return unknownImpl.AddRef();
    }

    STDMETHOD_(ULONG, Release)()
    {
        ULONG result = unknownImpl.Release();
        if (0 == result)
        {
            delete this;
        }
        return result;
    }

    // IClassFactory Interface
    STDMETHOD(CreateInstance)(IUnknown *pUnkOuter, REFIID riid, void **ppv)
    {
        HRESULT result = E_INVALIDARG;

        if (NULL != ppv)
        {
            T *obj = new T();

            if (NULL != obj)
            {
                result = obj->QueryInterface(riid, ppv);
            }
            else
            {
                *ppv = NULL;
                result = E_OUTOFMEMORY;
            }
        }

        return result;
    }

    STDMETHOD(LockServer)(BOOL fLock)
    {
        return CoLockObjectExternal(this, fLock, FALSE);
    }

private:
    UnknownImpl unknownImpl;
};

extern "C"
STDAPI DllRegisterServer()
{    
    register_manger.Register();
    return S_OK;
}

extern "C"
STDAPI DllUnregisterServer()
{ 
    register_manager.Unregister();
    return S_OK;
}

extern "C"
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void **ppv)
{
    HRESULT result = E_INVALIDARG; 
    IClassFactory *classFactory = NULL;

    if (NULL != ppv)
    {
        if (CLSID_SixelDecoder == rclsid)
        {
            result = S_OK;
            classFactory = new SixelCodecClassFactory<SixelDecoder>();
        }
        else
        {
            result = E_NOINTERFACE;
        }

        if (SUCCEEDED(result))
        {
            if (NULL != classFactory)
            {
                result = classFactory->QueryInterface(riid, ppv);
            }
            else
            {
                result = E_OUTOFMEMORY;
            }
        }
    }
    return result;    
}

extern "C"
BOOL WINAPI DllMain(HINSTANCE hinstDLL,
                    ULONG fdwReason,
                    LPVOID *lpvReserved)
{
    BOOL result = TRUE;

    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hinstDLL);
        break;

    case DLL_PROCESS_DETACH:
        break;
    }

    return result;
}

// EOF
