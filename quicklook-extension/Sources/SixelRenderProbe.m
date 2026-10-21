/* SPDX-License-Identifier: MIT
 * Explicit rendering diagnostics, never an installcheck side effect.
 * Image agreement is NOT selected-provider or cache provenance evidence.
 */
#import <Foundation/Foundation.h>
#import <QuickLookThumbnailing/QuickLookThumbnailing.h>
#import <ImageIO/ImageIO.h>
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
extern char **environ;

/* Hand-authored SIXEL: 16x12, opaque red left half and blue right half.
 * The oracle below does not decode SIXEL or use the extension's renderer.
 */
static const char specimen[] =
    "\033Pq\"1;1;16;12#0;2;100;0;0#1;2;0;0;100"
    "#0!8~#1!8~-#0!8~#1!8~\033\\";

static BOOL
matches(CGImageRef image)
{
    CGColorSpaceRef space;
    CGContextRef context;
    unsigned char pixels[16 * 12 * 4];
    unsigned char *pixel;
    size_t x, y;
    BOOL valid;

    if (!image || CGImageGetWidth(image) != 16 ||
        CGImageGetHeight(image) != 12)
        return NO;
    space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    context = CGBitmapContextCreate(pixels, 16, 12, 8, 16 * 4, space,
        kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(space);
    if (!context)
        return NO;
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake(0, 0, 16, 12), image);
    valid = YES;
    for (y = 0; y < 12; ++y) {
        for (x = 0; x < 16; ++x) {
            pixel = pixels + (y * 16 + x) * 4;
            /* Pure sRGB endpoints need no resampling/error allowance. */
            valid &= pixel[0] == (x < 8 ? 255 : 0) && pixel[1] == 0 &&
                pixel[2] == (x < 8 ? 0 : 255) && pixel[3] == 255;
        }
    }
    CGContextRelease(context);
    return valid;
}

static BOOL
check_file(NSString *path)
{
    CGImageSourceRef source;
    CGImageRef image;
    BOOL valid;

    source = CGImageSourceCreateWithURL(
        (__bridge CFURLRef)[NSURL fileURLWithPath:path], NULL);
    if (!source)
        return NO;
    image = CGImageSourceGetCount(source) == 1 ?
        CGImageSourceCreateImageAtIndex(source, 0, NULL) : NULL;
    valid = matches(image);
    if (image)
        CGImageRelease(image);
    CFRelease(source);
    return valid;
}

/* qlmanage does not promise a particular preview representation. Accept
 * exactly one PNG in its fresh output tree; HTML/PDF/ambiguous results are
 * unsupported evidence, not a reason to substitute a thumbnail result.
 */
static BOOL
check_preview(NSString *directory)
{
    NSDirectoryEnumerator *walk;
    NSString *relative, *candidate;
    NSUInteger count;
    BOOL isDirectory, failed;

    if (![[NSFileManager defaultManager] fileExistsAtPath:directory
        isDirectory:&isDirectory] || !isDirectory)
        return NO;
    failed = NO;
    candidate = nil;
    count = 0;
    walk = [[NSFileManager defaultManager] enumeratorAtPath:directory];
    for (relative in walk) {
        if (++count > 128) {
            failed = YES;
            break;
        }
        if ([[relative.pathExtension lowercaseString] isEqual:@"png"]) {
            if (candidate) {
                failed = YES;
                break;
            }
            candidate = [directory stringByAppendingPathComponent:relative];
        }
    }
    return !failed && candidate && check_file(candidate);
}

static int
preview(NSString *input, NSString *output)
{
    posix_spawn_file_actions_t actions;
    char *arguments[6];
    pid_t child, result;
    int status, error;
    NSTimeInterval deadline;

    /* A fresh output directory prevents accepting old diagnostic output.
     * It does not prove Quick Look's internal cache was bypassed.
     */
    if ([[NSFileManager defaultManager] fileExistsAtPath:output] ||
        ![[NSFileManager defaultManager] createDirectoryAtPath:output
        withIntermediateDirectories:NO attributes:nil error:NULL])
        return 1;
    arguments[0] = "/usr/bin/qlmanage";
    arguments[1] = "-p";
    arguments[2] = "-o";
    arguments[3] = (char *)output.fileSystemRepresentation;
    arguments[4] = (char *)input.fileSystemRepresentation;
    arguments[5] = NULL;
    error = posix_spawn_file_actions_init(&actions);
    if (error)
        return 1;
    error = posix_spawn_file_actions_adddup2(&actions, STDERR_FILENO,
        STDOUT_FILENO);
    if (!error)
        error = posix_spawn(&child, arguments[0], &actions, NULL,
            arguments, environ);
    posix_spawn_file_actions_destroy(&actions);
    if (error)
        return 1;
    deadline = NSProcessInfo.processInfo.systemUptime + 10;
    do {
        result = waitpid(child, &status, WNOHANG);
        if (result == child)
            return WIFEXITED(status) ? WEXITSTATUS(status) :
                WIFSIGNALED(status) ? 128 + WTERMSIG(status) : 1;
        if (result < 0 && errno != EINTR)
            return 1;
        usleep(5000);
    } while (NSProcessInfo.processInfo.systemUptime < deadline);
    /* The direct child has not been reaped: its PID is still ours. */
    kill(child, SIGKILL);
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
    return 124;
}

static int
thumbnail(NSString *input, BOOL *valid)
{
    QLThumbnailGenerationRequest *request;
    QLThumbnailGenerator *generator;
    dispatch_semaphore_t done;
    __block QLThumbnailRepresentation *result;
    __block NSError *failure;
    long status;

    request = [[QLThumbnailGenerationRequest alloc]
        initWithFileAtURL:[NSURL fileURLWithPath:input]
        size:CGSizeMake(16, 12) scale:1
        representationTypes:
        QLThumbnailGenerationRequestRepresentationTypeThumbnail];
    request.iconMode = NO;
    generator = QLThumbnailGenerator.sharedGenerator;
    done = dispatch_semaphore_create(0);
    result = nil;
    failure = nil;
    [generator generateBestRepresentationForRequest:request
        completionHandler:^(QLThumbnailRepresentation *representation,
                            NSError *error) {
            result = representation;
            failure = error;
            dispatch_semaphore_signal(done);
        }];
    status = dispatch_semaphore_wait(done,
        dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
    if (status) {
        [generator cancelRequest:request];
        return 124;
    }
    if (failure || !result) {
        fprintf(stderr, "thumbnail: %s\n",
            (failure.description ?: @"no representation").UTF8String);
        return 1;
    }
    *valid = result.type == QLThumbnailRepresentationTypeThumbnail &&
        matches(result.CGImage);
    return 0;
}

int
main(int argc, char **argv)
{
    @autoreleasepool {
        NSString *mode, *path;
        NSData *input, *expected, *json;
        BOOL valid, offline;
        int status;

        if (argc < 3 || argc > 4)
            return 64;
        mode = @(argv[1]);
        path = [@(argv[2]) stringByStandardizingPath];
        valid = NO;
        status = 0;
        offline = [mode hasPrefix:@"check-"];
        if ([mode isEqual:@"check-preview"] && argc == 3) {
            valid = check_preview(path);
        } else if ([mode isEqual:@"check-thumbnail"] && argc == 3) {
            valid = check_file(path);
        } else if (([mode isEqual:@"preview"] && argc == 4) ||
                   ([mode isEqual:@"thumbnail"] && argc == 3)) {
            expected = [NSData dataWithBytes:specimen
                length:sizeof(specimen) - 1];
            input = [NSData dataWithContentsOfFile:path];
            if (![input isEqual:expected] || !path.isAbsolutePath)
                return 64;
            if ([mode isEqual:@"preview"]) {
                if (![@(argv[3]) isAbsolutePath])
                    return 64;
                status = preview(path, @(argv[3]));
                valid = !status && check_preview(@(argv[3]));
            } else {
                status = thumbnail(path, &valid);
            }
        } else {
            return 64;
        }
        json = [NSJSONSerialization dataWithJSONObject:@{
            @"format": @1, @"mode": mode, @"offline": @(offline),
            @"commandStatus": @(status), @"imageMatches": @(valid),
            @"providerVerified": @NO, @"cacheProvenance": @"unavailable"
        } options:NSJSONWritingSortedKeys error:NULL];
        fwrite(json.bytes, 1, json.length, stdout);
        putchar('\n');
        /* A content match alone must never pass a live provider assertion. */
        return status ? status : !valid ? 1 : offline ? 0 : 2;
    }
}
