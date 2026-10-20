/* SPDX-License-Identifier: MIT */
/* Discover the installed codec through WIC, with no build-tree DLL loading. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <wchar.h>

int
main(void)
{
    IWICImagingFactory *factory = NULL;
    IWICStream *stream = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    HRESULT hr;
    UINT width = 0;
    UINT height = 0;
    BYTE rgba[4] = {0};
    BYTE sample[] = "\033Pq\"1;1;1;1#0;2;100;0;0#0@\033\\";
    WCHAR expected[32768];
    WCHAR loaded[32768];
    DWORD length;
    HMODULE module;
    int result = 1;

    length = GetEnvironmentVariableW(L"LIBSIXEL_WIC_INSTALLED_DLL",
                                    expected, 32768);
    if (length == 0 || length >= 32768) {
        fprintf(stderr, "Missing installed DLL path.\n");
        return 1;
    }
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        fprintf(stderr, "CoInitializeEx: 0x%08lx\n", (unsigned long)hr);
        return 1;
    }
    hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL,
                         CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                         (void **)&factory);
    if (FAILED(hr)) goto end;
    hr = IWICImagingFactory_CreateStream(factory, &stream);
    if (FAILED(hr)) goto end;
    hr = IWICStream_InitializeFromMemory(stream, sample, sizeof(sample) - 1);
    if (FAILED(hr)) goto end;
    /* No explicit codec CLSID: registry patterns must discover SIXEL. */
    hr = IWICImagingFactory_CreateDecoderFromStream(
        factory, (IStream *)stream, NULL, WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr)) goto end;
    module = GetModuleHandleW(L"libwicsixel.dll");
    length = module ? GetModuleFileNameW(module, loaded, 32768) : 0;
    if (length == 0 || length >= 32768 || _wcsicmp(loaded, expected) != 0) {
        fprintf(stderr, "WIC did not load the MSI's installed DLL.\n");
        goto end;
    }
    hr = IWICBitmapDecoder_GetFrame(decoder, 0, &frame);
    if (FAILED(hr)) goto end;
    hr = IWICBitmapFrameDecode_GetSize(frame, &width, &height);
    if (FAILED(hr)) goto end;
    hr = IWICImagingFactory_CreateFormatConverter(factory, &converter);
    if (FAILED(hr)) goto end;
    hr = IWICFormatConverter_Initialize(
        converter, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) goto end;
    hr = IWICFormatConverter_CopyPixels(converter, NULL, 4, 4, rgba);
    if (FAILED(hr)) goto end;
    /* Literal full-red input has an exact, opaque RGBA reference. */
    result = width != 1 || height != 1 || rgba[0] != 255 ||
             rgba[1] != 0 || rgba[2] != 0 || rgba[3] != 255;
    printf("WIC: %ux%u RGBA=%u,%u,%u,%u\n", width, height,
           rgba[0], rgba[1], rgba[2], rgba[3]);
end:
    if (FAILED(hr)) {
        fprintf(stderr, "WIC decode: 0x%08lx\n", (unsigned long)hr);
    }
    if (converter) IWICFormatConverter_Release(converter);
    if (frame) IWICBitmapFrameDecode_Release(frame);
    if (decoder) IWICBitmapDecoder_Release(decoder);
    if (stream) IWICStream_Release(stream);
    if (factory) IWICImagingFactory_Release(factory);
    CoUninitialize();
    return result;
}
