#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <CoreServices/CoreServices.h>

static void RegisterDefaultHandlerForUTI(NSString *identifier, CFStringRef handler, BOOL *hadFailure)
{
    if (identifier.length == 0) {
        return;
    }

    OSStatus status = LSSetDefaultRoleHandlerForContentType((__bridge CFStringRef)identifier,
                                                            kLSRolesViewer,
                                                            handler);
    NSString *handlerString = (__bridge NSString *)handler;

    if (status != noErr) {
        fprintf(stderr, "failed to set default handler for %s: %d\n", identifier.UTF8String, (int)status);
        if (hadFailure != NULL) {
            *hadFailure = YES;
        }
    } else {
        printf("Associated %s with %s\n", identifier.UTF8String, handlerString.UTF8String);
    }
}

int main(int argc, const char * argv[])
{
    (void)argc;
    (void)argv;

    @autoreleasepool {
        CFStringRef handler = CFSTR("com.saitoha.libsixel.quicklook.bridge");
        NSMutableOrderedSet<NSString *> *utis = [[NSMutableOrderedSet alloc] init];
        [utis addObject:@"com.saitoha.libsixel.sixel"];

        NSArray<NSString *> *extensions = @[ @"six", @"sixel" ];
        NSArray<NSString *> *mimeTypes = @[ @"image/x-sixel" ];

        for (NSString *ext in extensions) {
            if (@available(macOS 11.0, *)) {
                UTType *type = [UTType typeWithFilenameExtension:ext conformingToType:UTTypeImage];
                if (type != nil && type.identifier.length > 0) {
                    [utis addObject:type.identifier];
                }
            } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                CFStringRef identifier = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                                                               (__bridge CFStringRef)ext,
                                                                               NULL);
#pragma clang diagnostic pop
                if (identifier != NULL) {
                    [utis addObject:(__bridge_transfer NSString *)identifier];
                }
            }
        }

        for (NSString *mime in mimeTypes) {
            if (@available(macOS 11.0, *)) {
                UTType *type = [UTType typeWithMIMEType:mime];
                if (type != nil && type.identifier.length > 0) {
                    [utis addObject:type.identifier];
                }
            } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                CFStringRef identifier = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType,
                                                                               (__bridge CFStringRef)mime,
                                                                               NULL);
#pragma clang diagnostic pop
                if (identifier != NULL) {
                    [utis addObject:(__bridge_transfer NSString *)identifier];
                }
            }
        }

        BOOL hadFailure = NO;
        for (NSString *identifier in utis) {
            RegisterDefaultHandlerForUTI(identifier, handler, &hadFailure);
        }

        return hadFailure ? EXIT_FAILURE : EXIT_SUCCESS;
    }
}
