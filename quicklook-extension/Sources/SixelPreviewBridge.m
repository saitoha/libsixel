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
 * SixelPreviewBridge.m
 *
 * Helper application that converts SIXEL images to PNG and opens them with
 * Preview.app on behalf of the Quick Look extension.
 */
#import <Cocoa/Cocoa.h>

#import "SixelQuickLookShared.h"

/*
 * Application delegate responsible for handling open events and temporary
 * files.
 */
@interface SixelPreviewBridgeDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSMutableArray<NSURL *> *temporaryFiles;
@property (nonatomic, assign) BOOL handledOpenRequest;
@end

@implementation SixelPreviewBridgeDelegate

/*
 * Prepare containers for temporary files created during the session.
 */
- (instancetype)init
{
    self = [super init];
    if (self != nil) {
        _temporaryFiles = [[NSMutableArray alloc] init];
        _handledOpenRequest = NO;
    }
    return self;
}

/*
 * On launch, inspect command-line arguments and open any provided files.
 */
- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;

    NSArray<NSString *> *arguments = [[NSProcessInfo processInfo] arguments];
    if (!self.handledOpenRequest && arguments.count > 1) {
        NSArray<NSString *> *paths =
            [arguments subarrayWithRange:NSMakeRange(1, arguments.count - 1)];
        if ([self handlePaths:paths fromApplication:NSApp]) {
            [self scheduleTermination];
        }
    }
}

/*
 * Respond to Dock or Finder "open file" requests.
 */
- (void)application:(NSApplication *)application
         openFiles:(NSArray<NSString *> *)filenames
{
    self.handledOpenRequest = YES;
    if ([self handlePaths:filenames fromApplication:application]) {
        [application replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
        [self scheduleTermination];
    } else {
        [application replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
    }
}

/*
 * Iterate over incoming paths and open each SIXEL.
 * Returns YES if any succeeded.
 */
- (BOOL)handlePaths:(NSArray<NSString *> *)paths
    fromApplication:(NSApplication *)application
{
    BOOL handledAny = NO;
    for (NSString *path in paths) {
        if ([self openSixelAtPath:path]) {
            handledAny = YES;
        }
    }

    if (! handledAny) {
        dispatch_async(dispatch_get_main_queue(), ^{
            NSAlert *alert = [[NSAlert alloc] init];
            alert.messageText = @"Fail to open SIXEL image.";
            alert.informativeText = @"Fail to decode SIXEL image.";
            [alert addButtonWithTitle:@"Close"];
            [alert runModal];
            [application terminate:nil];
        });
        return NO;
    }

    return YES;
}

/*
 * Decode a single SIXEL file, write a PNG, and open it in Preview.
 */
- (BOOL)openSixelAtPath:(NSString *)path
{
    NSURL *fileURL = [NSURL fileURLWithPath:path];
    NSError *decodeError = nil;
    CGImageRef image =
        SixelQuickLookCreateImageFromURL(fileURL, &decodeError);
    if (image == NULL) {
        if (decodeError != nil) {
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[bridge] decode failed for %@: %@",
                              path,
                              decodeError.localizedDescription);
        } else {
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[bridge] decode failed for %@ with unknown "
                              @"error",
                              path);
        }
        return NO;
    }

    NSError *pngError = nil;
    NSData *pngData = SixelQuickLookCreatePNGData(image, &pngError);
    CGImageRelease(image);

    if (pngData == nil) {
        if (pngError != nil) {
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[bridge] PNG conversion failed for %@: %@",
                              path,
                              pngError.localizedDescription);
        } else {
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[bridge] PNG conversion failed for %@",
                              path);
        }
        return NO;
    }

    NSError *writeError = nil;
    NSURL *temporaryURL =
        [self writePNGData:pngData originalURL:fileURL error:&writeError];
    if (temporaryURL == nil) {
        if (writeError != nil) {
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[bridge] failed to write PNG for %@: %@",
                              path,
                              writeError.localizedDescription);
        } else {
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[bridge] failed to write PNG for %@",
                              path);
        }
        return NO;
    }

    if (![self openTemporaryURLInPreview:temporaryURL]) {
        return NO;
    }

    [self.temporaryFiles addObject:temporaryURL];
    return YES;
}

/*
 * Materialize a temporary PNG alongside metadata derived from the source name.
 */
- (NSURL *)writePNGData:(NSData *)data
             originalURL:(NSURL *)originalURL
                   error:(NSError **)error
{
    NSString *baseName =
        [[originalURL lastPathComponent] stringByDeletingPathExtension];
    if (baseName.length == 0) {
        baseName = @"image";
    }

    NSString *uuid = [[NSUUID UUID] UUIDString];
    NSString *fileName =
        [NSString stringWithFormat:@"%@-%@.png", baseName, uuid];
    NSString *cacheDir =
        [NSTemporaryDirectory()
            stringByAppendingPathComponent:@"libsixel-preview"];

    NSFileManager *fm = [NSFileManager defaultManager];
    if (![fm createDirectoryAtPath:cacheDir
          withIntermediateDirectories:YES
                           attributes:nil
                                error:error]) {
        return nil;
    }

    NSURL *outputURL = [NSURL fileURLWithPath:cacheDir];
    outputURL = [outputURL URLByAppendingPathComponent:fileName];

    if (![data writeToURL:outputURL options:NSDataWritingAtomic error:error]) {
        return nil;
    }

    return outputURL;
}

/*
 * Open the temporary PNG in Preview.app. Uses modern APIs when available.
 */
- (BOOL)openTemporaryURLInPreview:(NSURL *)url
{
    NSURL *previewAppURL =
        [NSURL fileURLWithPath:@"/System/Applications/Preview.app"
                   isDirectory:YES];
    __block BOOL reportedSuccess = YES;
    if (@available(macOS 11.0, *)) {
        NSWorkspaceOpenConfiguration *configuration =
            [NSWorkspaceOpenConfiguration configuration];
        configuration.activates = YES;
        [[NSWorkspace sharedWorkspace] openURLs:@[url]
                           withApplicationAtURL:previewAppURL
                               configuration:configuration
                            completionHandler:
                                ^(NSRunningApplication * _Nullable __unused app,
                                  NSError * _Nullable openError) {
            if (openError != nil) {
                SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                                  @"[bridge] failed to open %@ in Preview: %@",
                                  url.path,
                                  openError.localizedDescription);
            }
        }];
    } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        NSError *legacyError = nil;
        BOOL success = [[NSWorkspace sharedWorkspace]
            openURLs:@[url]
            withApplicationAtURL:previewAppURL
            options:NSWorkspaceLaunchDefault
            configuration:@{}
            error:&legacyError];
        if (!success && legacyError != nil) {
            SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                              @"[bridge] failed to open %@ in Preview: %@",
                              url.path,
                              legacyError.localizedDescription);
            reportedSuccess = NO;
        }
#pragma clang diagnostic pop
    }
    return reportedSuccess;
}

/*
 * Give Preview a moment to launch, then quit the helper.
 */
- (void)scheduleTermination
{
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW, (int64_t)(2 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
        [[NSApplication sharedApplication] terminate:nil];
    });
}

/*
 * Remove any temporary PNG files on shutdown.
 */
- (void)applicationWillTerminate:(NSNotification *)notification
{
    (void)notification;
    NSFileManager *fm = [NSFileManager defaultManager];
    for (NSURL *url in self.temporaryFiles) {
        NSError *error = nil;
        if (![fm removeItemAtURL:url error:&error]) {
            if (error != nil) {
                SixelQuickLookLog(OS_LOG_TYPE_ERROR,
                                  @"[bridge] failed to remove temporary file %@"
                                  @": %@",
                                  url.path,
                                  error.localizedDescription);
            }
        }
    }
    [self.temporaryFiles removeAllObjects];
}

@end

/*
 * Entry point for the helper application. Boots Cocoa and hands over to AppKit.
 */
int main(int argc, const char * argv[])
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        SixelPreviewBridgeDelegate *delegate =
            [[SixelPreviewBridgeDelegate alloc] init];
        [NSApp setDelegate:delegate];
        return NSApplicationMain(argc, argv);
    }
}
