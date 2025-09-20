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
 * SixelPreviewProvider.m
 *
 * Implements QLPreviewingController so SIXEL images can be rendered inside
 * Quick Look. Decoding work happens off the main thread and the UI is updated
 * on the main queue.
 */

#import "SixelPreviewProvider.h"

#import <UniformTypeIdentifiers/UTType.h>

#import "SixelQuickLookShared.h"

/*
 * Private property that keeps the NSImageView used for preview rendering.
 */
@interface SixelPreviewProvider ()
@property (strong) NSImageView *imageView;
@end

@implementation SixelPreviewProvider

/*
 * Construct the preview view hierarchy and configure scaling-friendly defaults.
 */
- (void)loadView
{
    NSView *container = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 640.0, 480.0)];
    container.wantsLayer = YES;

    NSImageView *imageView = [[NSImageView alloc] initWithFrame:container.bounds];
    imageView.imageScaling = NSImageScaleProportionallyUpOrDown;
    imageView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    imageView.editable = NO;
    imageView.allowsCutCopyPaste = NO;

    [container addSubview:imageView];

    self.imageView = imageView;
    self.view = container;
}

/*
 * Apply the decoded image to the UI and update the preferred content size.
 */
- (void)updateViewWithImage:(NSImage *)image
{
    self.currentImage = image;
    self.imageView.image = image;
    NSSize size = image.size;
    if (size.width > 0.0 && size.height > 0.0) {
        self.preferredContentSize = size;
    } else {
        self.preferredContentSize = NSMakeSize(640.0, 480.0);
    }
}

- (void)providePreviewForFileRequest:(QLFilePreviewRequest *)request
                    completionHandler:(void (^)(QLPreviewReply * _Nullable reply,
                                                NSError * _Nullable error))handler
{
    NSURL *url = request.fileURL;
    SixelQuickLookLog(OS_LOG_TYPE_DEFAULT, @"[preview] provide preview for %@", url.path);

    BOOL scoped = [url startAccessingSecurityScopedResource];

    /*
     * Helper block that re-enters the main queue after background decoding.
     */
    void (^deliver)(QLPreviewReply * _Nullable, NSError * _Nullable, NSImage * _Nullable) = ^(QLPreviewReply *reply, NSError *error, NSImage *image) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (image != nil) {
                [self updateViewWithImage:image];
            }
            handler(reply, error);
            if (scoped) {
                [url stopAccessingSecurityScopedResource];
            }
        });
    };

    /*
     * Schedule heavy decoding work off the Quick Look request thread.
     */
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *decodeError = nil;
        CGImageRef image = SixelQuickLookCreateImageFromURL(url, &decodeError);
        if (image == NULL) {
            if (decodeError != nil) {
                SixelQuickLookLog(OS_LOG_TYPE_ERROR, @"[preview] decode failed: %@", decodeError.localizedDescription);
            }
            deliver(nil, decodeError, nil);
            return;
        }

        CGSize imageSize = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
        if (imageSize.width <= 0.0 || imageSize.height <= 0.0) {
            CGImageRelease(image);
            NSError *error = SixelQuickLookMakeError(-3, @"Invalid SIXEL image dimensions");
            SixelQuickLookLog(OS_LOG_TYPE_ERROR, @"[preview] invalid size for %@", url.path);
            deliver(nil, error, nil);
            return;
        }

        NSError *pngError = nil;
        NSData *pngData = SixelQuickLookCreatePNGData(image, &pngError);
        NSImage *imageObj = SixelQuickLookCreateNSImage(image);
        CGImageRelease(image);

        if (pngData == nil) {
            if (pngError != nil) {
                SixelQuickLookLog(OS_LOG_TYPE_ERROR, @"[preview] PNG conversion failed: %@", pngError.localizedDescription);
            }
            deliver(nil, pngError, nil);
            return;
        }

        UTType *pngType = UTTypePNG;
        if (pngType == nil) {
            pngType = [UTType typeWithIdentifier:@"public.png"];
        }

        if (pngType == nil) {
            NSError *typeError = SixelQuickLookMakeError(-10, @"Failed to resolve PNG content type");
            SixelQuickLookLog(OS_LOG_TYPE_ERROR, @"[preview] failed to resolve PNG content type");
            deliver(nil, typeError, nil);
            return;
        }

        QLPreviewReply *reply = [[QLPreviewReply alloc] initWithDataOfContentType:pngType
                                                                      contentSize:imageSize
                                                               dataCreationBlock:^NSData * _Nullable (QLPreviewReply * _Nonnull __unused replyObj,
                                                                                                   NSError * _Nullable * _Nullable __unused errorPtr) {
            return [pngData copy];
        }];

        if (reply == nil) {
            NSError *replyError = SixelQuickLookMakeError(-11, @"Failed to create Quick Look preview");
            deliver(nil, replyError, nil);
            return;
        }

        deliver(reply, nil, imageObj);
    });
}

- (void)preparePreviewOfFileAtURL:(NSURL *)url completionHandler:(void (^)(NSError * _Nullable))handler
{
    SixelQuickLookLog(OS_LOG_TYPE_DEFAULT, @"[preview] prepare preview for %@", url.path);

    BOOL scoped = [url startAccessingSecurityScopedResource];

    /*
     * Decode the image ahead of time to provide a fast initial render.
     */
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *decodeError = nil;
        CGImageRef image = SixelQuickLookCreateImageFromURL(url, &decodeError);

        if (image == NULL) {
            if (decodeError != nil) {
                SixelQuickLookLog(OS_LOG_TYPE_ERROR, @"[preview] decode failed: %@", decodeError.localizedDescription);
            }
            dispatch_async(dispatch_get_main_queue(), ^{
                handler(decodeError);
                if (scoped) {
                    [url stopAccessingSecurityScopedResource];
                }
            });
            return;
        }

        NSImage *imageObj = SixelQuickLookCreateNSImage(image);
        CGImageRelease(image);

        dispatch_async(dispatch_get_main_queue(), ^{
            if (imageObj != nil) {
                [self updateViewWithImage:imageObj];
                handler(nil);
            } else {
                NSError *error = SixelQuickLookMakeError(-5, @"Failed to create NSImage");
                handler(error);
            }
            if (scoped) {
                [url stopAccessingSecurityScopedResource];
            }
        });
    });
}

@end
