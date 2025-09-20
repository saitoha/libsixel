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
 * SixelQuickLookShared.h
 *
 * Declarations for functionality shared between the preview and thumbnail
 * extensions: SIXEL decoding, PNG conversion, and rendering helpers.
 */

#ifndef SIXEL_QUICKLOOK_SHARED_H
#define SIXEL_QUICKLOOK_SHARED_H

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#if TARGET_OS_OSX
#import <AppKit/AppKit.h>
#endif
#import <os/log.h>

NS_ASSUME_NONNULL_BEGIN

extern NSErrorDomain const SixelQuickLookErrorDomain;

/* Build an NSError scoped to the libsixel Quick Look domain. */
NSError *SixelQuickLookMakeError(NSInteger code, NSString *message);
/* Simple os_log wrapper for extension diagnostics. */
void SixelQuickLookLog(os_log_type_t type, NSString *format, ...);

/* Load a SIXEL image from an NSURL and return a CGImage. */
CGImageRef _Nullable SixelQuickLookCreateImageFromURL(NSURL *url, NSError **error_out);
/* Convert a CGImage into PNG data. */
NSData * _Nullable SixelQuickLookCreatePNGData(CGImageRef image, NSError **error_out);
/* Fit the image dimensions within a bounding box. */
CGSize SixelQuickLookFitSize(CGSize imageSize, CGSize maxSize);
/* Draw a CGImage into a CGContext, optionally flipping vertically. */
void SixelQuickLookDrawImage(CGContextRef context, CGImageRef image, CGSize size, BOOL flipVertically);
#if TARGET_OS_OSX
/* Create an NSImage from a CGImage. */
NSImage * _Nullable SixelQuickLookCreateNSImage(CGImageRef image);
#endif

NS_ASSUME_NONNULL_END

#endif /* SIXEL_QUICKLOOK_SHARED_H */
