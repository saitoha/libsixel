/*
 * Copyright (c) 2025 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
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

/*
 * libsixel Quick Look extension
 * SixelQuickLookShared.m
 *
 * Shared SIXEL decoding and rendering utilities used by the preview and
 * thumbnail extensions. Bridges decoded data into Core Graphics primitives.
 */

#import "SixelQuickLookShared.h"

#import "config.h"

#import <ImageIO/ImageIO.h>

#import <limits.h>
#import <stdlib.h>
#import <string.h>

#import <sixel.h>

/* Error domain used by the libsixel Quick Look utilities. */
NSErrorDomain const SixelQuickLookErrorDomain = @"com.saitoha.libsixel.quicklook";

/*
 * Build an NSError tailored to the Quick Look error domain.
 */
NSError *
SixelQuickLookMakeError(NSInteger code, NSString *message)
{
    NSDictionary *userInfo = @{ NSLocalizedDescriptionKey : message };
    return [NSError errorWithDomain:SixelQuickLookErrorDomain code:code userInfo:userInfo];
}

/*
 * Emit Quick Look related diagnostics via os_log.
 */
void
SixelQuickLookLog(os_log_type_t type, NSString *format, ...)
{
    va_list args;
    va_start(args, format);
    NSString *message = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);

    if (message != nil) {
        os_log_with_type(OS_LOG_DEFAULT, type, "%{public}@", message);
    }
}

/*
 * Tracks allocation metadata so CGDataProvider cleanup can release SIXEL
 * buffers.
 */
typedef struct {
    sixel_allocator_t *allocator;
    unsigned char *data;
} SixelQuickLookProviderInfo;

/*
 * Cleanup hook invoked when the CGDataProvider is released.
 */
static void
SixelQuickLookProviderRelease(void *info, const void *data, size_t size)
{
    (void)data;
    (void)size;

    SixelQuickLookProviderInfo *provider = (SixelQuickLookProviderInfo *)info;
    if (provider == NULL) {
        return;
    }

    if (provider->allocator != NULL && provider->data != NULL) {
        sixel_allocator_free(provider->allocator, provider->data);
    }

    if (provider->allocator != NULL) {
        sixel_allocator_unref(provider->allocator);
    }

    free(provider);
}

/*
 * Translate a libsixel status code into an NSError, preferring detailed
 * messages.
 */
static NSError *
SixelQuickLookNSErrorFromStatus(SIXELSTATUS status, NSString *fallback)
{
    const char *message = sixel_helper_get_additional_message();
    NSString *localized = nil;
    if (message != NULL && message[0] != '\0') {
        localized = [NSString stringWithUTF8String:message];
    }
    if (localized == nil) {
        localized = fallback != nil ? fallback : @"Failed to decode SIXEL data";
    }
    return SixelQuickLookMakeError((NSInteger)status, localized);
}

static CGImageRef
SixelQuickLookCreateImageFromData(NSData *data, NSError **error_out)
{
    if (error_out != NULL) {
        *error_out = nil;
    }

    if (data == nil || data.length == 0) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-2, @"SIXEL data is empty");
        }
        return NULL;
    }

    const unsigned char *bytes = data.bytes;
    size_t length = data.length;
    if (length > (size_t)INT_MAX) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-3, @"SIXEL data is too large");
        }
        return NULL;
    }

    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status = sixel_allocator_new(&allocator, malloc, calloc, realloc, free);
    if (SIXEL_FAILED(status)) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookNSErrorFromStatus(status, @"Failed to initialize memory allocator");
        }
        return NULL;
    }

    unsigned char *indices = NULL;
    unsigned char *palette = NULL;
    unsigned char *rgba = NULL;
    int width = 0;
    int height = 0;
    int ncolors = 0;
    CGImageRef image = NULL;

    status = sixel_decode_raw((unsigned char *)bytes,
                              (int)length,
                              &indices,
                              &width,
                              &height,
                              &palette,
                              &ncolors,
                              allocator);
    if (SIXEL_FAILED(status)) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookNSErrorFromStatus(status, @"Failed to decode SIXEL data");
        }
        goto cleanup;
    }

    if (width <= 0 || height <= 0) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-4, @"Decoded image dimensions are invalid");
        }
        goto cleanup;
    }

    size_t pixelCount = (size_t)width * (size_t)height;
    rgba = sixel_allocator_malloc(allocator, pixelCount * 4);
    if (rgba == NULL) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-7, @"Failed to allocate RGBA buffer");
        }
        goto cleanup;
    }

    for (size_t i = 0; i < pixelCount; ++i) {
        unsigned char index = indices[i];
        unsigned char r = 0;
        unsigned char g = 0;
        unsigned char b = 0;

        if ((int)((unsigned int)index) < ncolors) {
            size_t base = (size_t)index * 3u;
            r = palette[base + 0];
            g = palette[base + 1];
            b = palette[base + 2];
        }

        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = 255;
    }

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    if (colorSpace == NULL) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-8, @"Failed to create RGB color space");
        }
        goto cleanup;
    }

    SixelQuickLookProviderInfo *providerInfo = calloc(1, sizeof(*providerInfo));
    if (providerInfo == NULL) {
        CGColorSpaceRelease(colorSpace);
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-7, @"Failed to allocate context for data provider");
        }
        goto cleanup;
    }

    providerInfo->allocator = allocator;
    providerInfo->data = rgba;
    sixel_allocator_ref(allocator);

    size_t stride = (size_t)width * 4u;
    CGDataProviderRef provider = CGDataProviderCreateWithData(providerInfo,
                                                             rgba,
                                                             stride * (size_t)height,
                                                             SixelQuickLookProviderRelease);
    if (provider == NULL) {
        sixel_allocator_unref(allocator);
        free(providerInfo);
        CGColorSpaceRelease(colorSpace);
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-9, @"Failed to create CGDataProvider");
        }
        goto cleanup;
    }

    image = CGImageCreate((size_t)width,
                          (size_t)height,
                          8,
                          32,
                          stride,
                          colorSpace,
                          kCGBitmapByteOrderDefault | kCGImageAlphaLast,
                          provider,
                          NULL,
                          false,
                          kCGRenderingIntentDefault);

    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);

    if (image == NULL) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-10, @"Failed to create CGImage");
        }
        goto cleanup;
    }

    /* ownership transferred to provider */
    rgba = NULL;

cleanup:
    if (palette != NULL) {
        sixel_allocator_free(allocator, palette);
    }
    if (indices != NULL) {
        sixel_allocator_free(allocator, indices);
    }
    if (rgba != NULL) {
        sixel_allocator_free(allocator, rgba);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }

    return image;
}

CGImageRef
SixelQuickLookCreateImageFromURL(NSURL *url, NSError **error_out)
{
    if (error_out != NULL) {
        *error_out = nil;
    }

    NSError *readError = nil;
    NSData *data = [NSData dataWithContentsOfURL:url options:NSDataReadingMappedIfSafe error:&readError];
    if (data == nil) {
        if (error_out != NULL) {
            *error_out = readError;
        }
        return NULL;
    }

    NSError *decodeError = nil;
    CGImageRef image = SixelQuickLookCreateImageFromData(data, &decodeError);
    if (image == NULL && error_out != NULL) {
        *error_out = decodeError;
    }
    return image;
}

NSData *
SixelQuickLookCreatePNGData(CGImageRef image, NSError **error_out)
{
    if (error_out != NULL) {
        *error_out = nil;
    }

    if (image == NULL) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-6, @"Image data is missing");
        }
        return nil;
    }

    NSMutableData *data = [NSMutableData data];
    if (data == nil) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-7, @"Failed to allocate buffer for PNG data");
        }
        return nil;
    }

    CGImageDestinationRef destination = CGImageDestinationCreateWithData((__bridge CFMutableDataRef)data, CFSTR("public.png"), 1, NULL);
    if (destination == NULL) {
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-8, @"Failed to initialize PNG encoder");
        }
        return nil;
    }

    CGImageDestinationAddImage(destination, image, NULL);
    if (!CGImageDestinationFinalize(destination)) {
        CFRelease(destination);
        if (error_out != NULL) {
            *error_out = SixelQuickLookMakeError(-9, @"Failed to write PNG data");
        }
        return nil;
    }

    CFRelease(destination);
    return data;
}

#if TARGET_OS_OSX
NSImage *
SixelQuickLookCreateNSImage(CGImageRef image)
{
    if (image == NULL) {
        return nil;
    }

    CGSize size = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
    NSImage *result = [[NSImage alloc] initWithCGImage:image size:size.width > 0 && size.height > 0 ? size : NSZeroSize];
    return result;
}
#endif

CGSize
SixelQuickLookFitSize(CGSize imageSize, CGSize maxSize)
{
    CGFloat width = imageSize.width;
    CGFloat height = imageSize.height;

    if (width <= 0.0 || height <= 0.0) {
        return CGSizeZero;
    }

    if (width > maxSize.width && maxSize.width > 0.0) {
        CGFloat scale = maxSize.width / width;
        width *= scale;
        height *= scale;
    }

    if (height > maxSize.height && maxSize.height > 0.0) {
        CGFloat scale = maxSize.height / height;
        width *= scale;
        height *= scale;
    }

    return CGSizeMake(width, height);
}

void
SixelQuickLookDrawImage(CGContextRef context, CGImageRef image, CGSize size, BOOL flipVertically)
{
    CGRect rect = CGRectMake(0.0f, 0.0f, size.width, size.height);

    CGContextSaveGState(context);
    if (flipVertically) {
        CGContextTranslateCTM(context, 0.0f, size.height);
        CGContextScaleCTM(context, 1.0f, -1.0f);
    }
    CGContextDrawImage(context, rect, image);
    CGContextRestoreGState(context);
}
