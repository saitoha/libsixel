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
 * SixelThumbnailProvider.m
 *
 * Implements QLThumbnailProvider so Quick Look can request SIXEL thumbnails.
 * Decodes the image, scales it to the requested size, and draws into a context.
 */

#import "SixelThumbnailProvider.h"

#import "SixelQuickLookShared.h"

@implementation SixelThumbnailProvider

- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request
                     completionHandler:
                         (void (^)(QLThumbnailReply * _Nullable reply,
                                   NSError * _Nullable error))handler
{
    NSURL *fileURL = request.fileURL;
    CGSize maximumSize = request.maximumSize;
    SixelQuickLookLog(OS_LOG_TYPE_DEFAULT,
                      @"[thumbnail] request for %@ (max %.0fx%.0f)",
                      fileURL.path,
                      maximumSize.width,
                      maximumSize.height);

    BOOL scoped = [fileURL startAccessingSecurityScopedResource];
    /* Forward results to Quick Look on the main queue once processing
     * finishes. */
    void (^deliver)(QLThumbnailReply *_Nullable, NSError *_Nullable) =
        ^(QLThumbnailReply *reply, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            handler(reply, error);
        });
    };
    @try {
        /* Decode the SIXEL image off the main queue to obtain CGImage data. */
        NSError *decodeError = nil;
        CGImageRef image =
            SixelQuickLookCreateImageFromURL(fileURL, &decodeError);
        if (image == NULL) {
            if (decodeError != nil) {
                SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                                  @"[thumbnail] decode failed: %@",
                                  decodeError.localizedDescription);
            } else {
                SixelQuickLookLog(
                    OS_LOG_TYPE_ERROR,
                    @"[thumbnail] decode failed with unknown error");
            }
            deliver(nil, decodeError);
            return;
        }

        CGSize imageSize = CGSizeMake(CGImageGetWidth(image),
                                      CGImageGetHeight(image));
        if (imageSize.width <= 0.0 || imageSize.height <= 0.0) {
            CGImageRelease(image);
            NSError *error =
                SixelQuickLookMakeError(-3, @"Invalid SIXEL image dimensions");
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[thumbnail] invalid size for %@",
                              fileURL.path);
            deliver(nil, error);
            return;
        }

        CGSize fitted = SixelQuickLookFitSize(imageSize, maximumSize);
        if (fitted.width <= 0.0 || fitted.height <= 0.0) {
            CGImageRelease(image);
            NSError *error = SixelQuickLookMakeError(
                -4, @"Failed to compute thumbnail size");
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[thumbnail] fit failure for %@",
                              fileURL.path);
            deliver(nil, error);
            return;
        }

        /* Retain the decoded image for use inside the drawing block. */
        CGImageRef imageForBlock = CGImageRetain(image);
        QLThumbnailReply *reply =
            [QLThumbnailReply replyWithContextSize:fitted
                                     drawingBlock:^BOOL(CGContextRef context) {
                                         size_t width =
                                             CGBitmapContextGetWidth(context);
                                         size_t height =
                                             CGBitmapContextGetHeight(context);
                                         CGSize contextSize = CGSizeMake(
                                             (CGFloat)width, (CGFloat)height);
                                         /* Draw the decoded image into the
                                          * thumbnail context. */
                                         SixelQuickLookDrawImage(context,
                                                                 imageForBlock,
                                                                 contextSize,
                                                                 NO);
                                         CGImageRelease(imageForBlock);
                                         return YES;
                                     }];

        CGImageRelease(image);
        SixelQuickLookLog(OS_LOG_TYPE_INFO,
                          @"[thumbnail] reply prepared for %@ (%.0fx%.0f)",
                          fileURL.path,
                          fitted.width,
                          fitted.height);
        deliver(reply, nil);
    } @finally {
        if (scoped) {
            [fileURL stopAccessingSecurityScopedResource];
        }
    }
}

@end
