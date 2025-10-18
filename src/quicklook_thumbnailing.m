/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2025 libsixel developers.
 */

#include "config.h"

#if HAVE_QUICKLOOK_THUMBNAILING

#import <Foundation/Foundation.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import <dispatch/dispatch.h>

#include <CoreGraphics/CoreGraphics.h>

CGImageRef
sixel_quicklook_thumbnail_create(CFURLRef url, CGSize max_size)
{
    __block CGImageRef image = NULL;

    if (url == NULL) {
        return NULL;
    }

    @autoreleasepool {
        NSURL *file_url = (__bridge NSURL *)url;
        if (file_url == nil) {
            return NULL;
        }

        QLThumbnailGenerator *generator =
            [QLThumbnailGenerator sharedGenerator];
        if (generator == nil) {
            return NULL;
        }

        QLThumbnailGenerationRequest *request =
            [[QLThumbnailGenerationRequest alloc]
                initWithFileAtURL:file_url
                             size:max_size
                            scale:1.0f
              representationTypes:
                  QLThumbnailGenerationRequestRepresentationTypeThumbnail];
        if (request == nil) {
            return NULL;
        }

        if (max_size.width < max_size.height) {
            request.minimumDimension = (CGFloat)max_size.width;
        } else {
            request.minimumDimension = (CGFloat)max_size.height;
        }
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        if (semaphore == NULL) {
            [request release];
            return NULL;
        }

        [generator generateBestRepresentationForRequest:request
            completionHandler:^(QLThumbnailRepresentation *thumbnail,
                                NSError *error) {
            if (thumbnail != nil) {
                CGImageRef cgimage = thumbnail.CGImage;
                if (cgimage != NULL) {
                    image = CGImageRetain(cgimage);
                }
            }
            (void)error;
            dispatch_semaphore_signal(semaphore);
        }];

        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
        dispatch_release(semaphore);
        [request release];
    }

    return image;
}

#endif  /* HAVE_QUICKLOOK_THUMBNAILING */
