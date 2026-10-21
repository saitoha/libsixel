/* Build, inspect and explicitly manage the fixed Quick Look package. */
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 110000
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#endif
#import <Security/Security.h>
#import <Security/AuthSession.h>
#import <CoreServices/CoreServices.h>
#import <SystemConfiguration/SystemConfiguration.h>
#import <CommonCrypto/CommonDigest.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <dispatch/dispatch.h>

#ifndef SIXEL_INSTALL_ROOT
#define SIXEL_INSTALL_ROOT @"/"
#define SIXEL_INSTALLER @"/usr/sbin/installer"
#define SIXEL_PKGUTIL @"/usr/sbin/pkgutil"
#define SIXEL_INSTALL_UID geteuid()
#endif

static NSString *const package_id = @"com.saitoha.libsixel.quicklook.pkg";
static NSString *const support_path =
    @"Library/Application Support/libsixel/QuickLook";

static void
require(BOOL condition, NSString *message)
{
    if (!condition) {
        [NSException raise:@"QuickLookPackage" format:@"%@", message];
    }
}

static double
monotonic_seconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)now.tv_sec + (double)now.tv_nsec / 1000000000.0;
}

static void
report_phase(const char *name, double *started)
{
    double now;
    now = monotonic_seconds();
    fprintf(stderr, "Quick Look package phase %s: %.3f s\n",
            name, now - *started);
    *started = now;
}

static NSArray *
owned_roots(void)
{
    return @[@"Applications/SixelQuickLookHost.app",
             @"Applications/SixelPreviewBridge.app", support_path];
}

static NSArray *
bundle_paths(void)
{
    return @[(@"Applications/SixelQuickLookHost.app/Contents/PlugIns/"
              @"SixelPreview.appex"),
             (@"Applications/SixelQuickLookHost.app/Contents/PlugIns/"
              @"SixelThumbnail.appex"),
             @"Applications/SixelQuickLookHost.app",
             @"Applications/SixelPreviewBridge.app"];
}

static NSData *
read_data(NSString *path)
{
    NSData *data;
    data = [NSData dataWithContentsOfFile:path];
    require(data != nil, [@"Cannot read " stringByAppendingString:path]);
    return data;
}

static NSString *
hex_data(NSData *data)
{
    NSMutableString *text;
    const unsigned char *bytes;
    NSUInteger i;
    text = [NSMutableString string];
    bytes = data.bytes;
    for (i = 0; i < data.length; ++i) {
        [text appendFormat:@"%02x", bytes[i]];
    }
    return text;
}

static NSString *
hash_file(NSString *path)
{
    NSData *data;
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    data = read_data(path);
    require(data.length <= UINT_MAX, @"Payload file exceeds hash size limit");
    CC_SHA256(data.bytes, (CC_LONG)data.length, digest);
    return hex_data([NSData dataWithBytes:digest length:sizeof(digest)]);
}

static void
write_data(NSData *data, NSString *path)
{
    require(data != nil && [data writeToFile:path atomically:YES],
            [@"Cannot write " stringByAppendingString:path]);
}

static void
write_json(id object, NSString *path)
{
    write_data([NSJSONSerialization dataWithJSONObject:object
        options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys
        error:nil], path);
}

static NSDictionary *
read_json(NSString *path)
{
    id object;
    object = [NSJSONSerialization JSONObjectWithData:read_data(path)
        options:0 error:nil];
    require([object isKindOfClass:[NSDictionary class]], @"Invalid JSON");
    return object;
}

static void
make_directory(NSString *path)
{
    require([[NSFileManager defaultManager] createDirectoryAtPath:path
        withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:
        @0755} error:nil], @"Cannot create staging directory");
}

static void
copy_file(NSString *from, NSString *to)
{
    make_directory([to stringByDeletingLastPathComponent]);
    require([[NSFileManager defaultManager] copyItemAtPath:from toPath:to
        error:nil], [@"Cannot copy " stringByAppendingString:from]);
}

static NSString *
run(NSString *program, NSArray *arguments, int *status)
{
    NSTask *task;
    NSPipe *pipe;
    dispatch_semaphore_t terminated;
    NSData *data;
    NSString *output;
    task = [[NSTask alloc] init];
    pipe = [NSPipe pipe];
    terminated = dispatch_semaphore_create(0);
    /* NSTask's run-loop wait can add a polling interval after pipe EOF.
     * The termination handler signals actual process completion directly;
     * tool startup, output collection and termination stay inside the call. */
    task.terminationHandler = ^(NSTask *finished) {
        (void)finished;
        dispatch_semaphore_signal(terminated);
    };
    task.executableURL = [NSURL fileURLWithPath:program];
    task.arguments = arguments;
    task.standardOutput = pipe;
    task.standardError = pipe;
    require([task launchAndReturnError:nil],
            [@"Cannot execute " stringByAppendingString:program]);
    /* Drain before waiting: tool output must not fill a pipe and deadlock. */
    data = [pipe.fileHandleForReading readDataToEndOfFile];
    dispatch_semaphore_wait(terminated, DISPATCH_TIME_FOREVER);
    output = [[NSString alloc] initWithData:data
        encoding:NSUTF8StringEncoding];
    if (status != NULL) {
        *status = task.terminationStatus;
    } else {
        require(task.terminationStatus == 0,
            [NSString stringWithFormat:@"%@ failed: %@", program, output]);
    }
    return output ?: @"";
}

/* Set the creation mask in the signing child only. The parent retains its
 * caller's mask even if launch or signing fails; other packaging operations
 * never inherit this override. Pass arguments separately, not as shell code.
 * codesign creates CodeResources as 0644 before sealing each private bundle;
 * do not repair permissions after the final signature has been produced. */
static void
sign_private(NSArray *arguments)
{
    NSMutableArray *command;
    command = [@[@"-c", @"umask 022 && exec /usr/bin/codesign \"$@\"",
        @"codesign"] mutableCopy];
    [command addObjectsFromArray:arguments];
    run(@"/bin/sh", command, NULL);
}

static NSArray *
version_parts(NSString *version)
{
    NSArray *parts;
    NSString *part;
    NSUInteger i;
    NSInteger limit;
    require([version isKindOfClass:[NSString class]], @"Missing version");
    parts = [version componentsSeparatedByString:@"."];
    require(parts.count == 3, @"Version must contain three decimal integers");
    i = 0;
    for (part in parts) {
        limit = i == 0 ? 9999 : 99;
        require(part.length > 0 &&
            [part rangeOfCharacterFromSet:[[NSCharacterSet
                decimalDigitCharacterSet] invertedSet]].location == NSNotFound
            && [part isEqualToString:[NSString stringWithFormat:@"%ld",
                (long)part.integerValue]] && part.integerValue <= limit
            && (i != 0 || part.integerValue > 0),
            @"Version requires canonical M.m.p (1..9999, 0..99, 0..99)");
        ++i;
    }
    return parts;
}

static NSComparisonResult
compare_versions(NSString *a, NSString *b)
{
    return [a compare:b options:NSNumericSearch];
}

static NSString *
manifest_path(NSString *root)
{
    return [root stringByAppendingPathComponent:
        [support_path stringByAppendingPathComponent:@"payload-manifest.json"]];
}

/* Only these three trees are owned. Shared parent directories are not owned.
 * Reject symlinks, including ancestors, so root preflight cannot read through
 * an unowned link. The current payload has no need for symbolic links. */
static void
check_ancestors(NSString *root, NSString *relative)
{
    NSString *part;
    NSString *path;
    struct stat attributes;
    path = root;
    for (part in [relative pathComponents]) {
        require(![part isEqualToString:@".."] &&
            ![part isEqualToString:@"/"], @"Unsafe payload path");
        path = [path stringByAppendingPathComponent:part];
        /* A fresh install may have missing destination ancestors. Still
         * reject dangling links; do not mistake a stat error for absence. */
        if (lstat(path.fileSystemRepresentation, &attributes) != 0) {
            require(errno == ENOENT, @"Cannot stat payload ancestor");
            continue;
        }
        require(!S_ISLNK(attributes.st_mode),
                [@"Symbolic link is not allowed: "
                    stringByAppendingString:path]);
    }
}

static NSArray *
payload_entries_for_roots(NSString *root, NSArray *roots)
{
    NSFileManager *fm;
    NSMutableArray *paths;
    NSMutableArray *entries;
    NSDirectoryEnumerator *enumerator;
    NSString *owned;
    NSString *child;
    NSString *relative;
    NSString *path;
    struct stat attributes;
    NSMutableDictionary *entry;
    fm = [NSFileManager defaultManager];
    paths = [NSMutableArray array];
    entries = [NSMutableArray array];
    for (owned in roots) {
        check_ancestors(root, owned);
        path = [root stringByAppendingPathComponent:owned];
        require([fm fileExistsAtPath:path], @"Missing owned payload tree");
        [paths addObject:owned];
        enumerator = [fm enumeratorAtPath:path];
        require(enumerator != nil, @"Cannot enumerate payload tree");
        for (child in enumerator) {
            [paths addObject:[owned stringByAppendingPathComponent:child]];
        }
    }
    [paths sortUsingSelector:@selector(compare:)];
    for (relative in paths) {
        path = [root stringByAppendingPathComponent:relative];
        /* Ancestors above each owned root were checked before enumeration.
         * Sorted paths visit every owned directory before its children, so
         * inspect each node once instead of re-statting all its ancestors.
         * Only numeric type/mode/ownership are needed: lstat avoids the
         * broader Foundation attribute lookup and never follows a symlink.
         * Do not skip the manifest's type check: its hash alone is excluded. */
        require(lstat(path.fileSystemRepresentation, &attributes) == 0,
                @"Cannot stat payload");
        require(!S_ISLNK(attributes.st_mode),
                @"Symbolic link is not allowed in payload");
        if ([path isEqualToString:manifest_path(root)]) {
            require(S_ISREG(attributes.st_mode),
                    @"Manifest must be a regular file");
            continue;
        }
        entry = [@{@"path":relative,
            @"mode":@(attributes.st_mode & 07777)} mutableCopy];
        if (S_ISDIR(attributes.st_mode)) {
            /* Existing directories are deliberately not repaired by Installer.
             * Reject incompatible owned attributes before accepting an update.
             * Private fixture roots are not installed system-owned trees. */
            require((attributes.st_mode & 07777) == 0755,
                    @"Owned directory mode must be 0755");
            if ([root isEqual:@"/"]) {
                require(attributes.st_uid == 0 && attributes.st_gid == 0,
                    @"Installed owned directory must be root:wheel");
            }
            entry[@"kind"] = @"directory";
        } else {
            require(S_ISREG(attributes.st_mode),
                    @"Unsupported payload file type");
            entry[@"kind"] = @"file";
            entry[@"sha256"] = hash_file(path);
        }
        [entries addObject:entry];
    }
    return entries;
}

static NSArray *
payload_entries(NSString *root)
{
    return payload_entries_for_roots(root, owned_roots());
}

static void
validate_manifest(NSDictionary *manifest)
{
    NSMutableSet *paths;
    NSDictionary *code;
    NSMutableSet *expected;
    NSMutableDictionary *resources;
    NSString *relative;
    NSDictionary *entry;
    require([manifest[@"format"] isEqual:@1] &&
        [manifest[@"packageIdentifier"] isEqual:package_id] &&
        [manifest[@"files"] isKindOfClass:[NSArray class]],
        @"Unsupported payload manifest");
    version_parts(manifest[@"packageVersion"]);
    require([manifest[@"codes"] isKindOfClass:[NSArray class]] &&
        [manifest[@"codes"] count] == 6, @"Invalid manifest code inventory");
    expected = [NSMutableSet setWithArray:bundle_paths()];
    [expected addObject:[support_path
        stringByAppendingPathComponent:@"register_sixel_preview"]];
    [expected addObject:[support_path
        stringByAppendingPathComponent:@"sixel-quicklook-package"]];
    paths = [NSMutableSet set];
    for (code in manifest[@"codes"]) {
        require([code isKindOfClass:[NSDictionary class]] &&
            [code[@"path"] isKindOfClass:[NSString class]],
            @"Invalid manifest code path");
        [paths addObject:code[@"path"]];
    }
    require([paths isEqual:expected], @"Unexpected manifest code paths");
    /* Distribution permissions are independent of recorded self-consistency.
     * Require all four signature resources, even if both manifest and Bom
     * would otherwise agree on an unreadable mode or an omitted resource. */
    resources = [NSMutableDictionary dictionary];
    for (relative in bundle_paths()) {
        resources[[relative stringByAppendingPathComponent:
            @"Contents/_CodeSignature/CodeResources"]] = @0;
    }
    for (entry in manifest[@"files"]) {
        relative = entry[@"path"];
        if (resources[relative] != nil) {
            require([resources[relative] isEqual:@0] &&
                [entry[@"kind"] isEqual:@"file"] &&
                [entry[@"mode"] isEqual:@0644],
                @"Signature resource must be a unique regular file at 0644");
            resources[relative] = @1;
        }
    }
    require(![resources.allValues containsObject:@0],
            @"Missing signature resource in manifest");
}

static void
verify_payload(NSString *root, NSDictionary *manifest)
{
    validate_manifest(manifest);
    require([payload_entries(root) isEqual:manifest[@"files"]],
        @"Payload differs from manifest (missing, extra, changed or mode)");
}

static NSDictionary *
signature(NSString *path)
{
    SecStaticCodeRef code;
    CFDictionaryRef information;
    NSDictionary *info;
    NSDictionary *result;
    OSStatus status;
    code = NULL;
    information = NULL;
    status = SecStaticCodeCreateWithPath((__bridge CFURLRef)
        [NSURL fileURLWithPath:path], kSecCSDefaultFlags, &code);
    require(status == errSecSuccess, @"Cannot open signed code");
    status = SecStaticCodeCheckValidity(code,
        kSecCSStrictValidate | kSecCSCheckAllArchitectures, NULL);
    if (status != errSecSuccess) {
        CFRelease(code);
        require(NO, [@"Invalid code signature: "
            stringByAppendingString:path]);
    }
    status = SecCodeCopySigningInformation(code,
        kSecCSSigningInformation, &information);
    CFRelease(code);
    require(status == errSecSuccess, @"Cannot read signature identity");
    info = CFBridgingRelease(information);
    require(info[(__bridge NSString *)kSecCodeInfoUnique] != nil,
            @"Missing code directory hash");
    result = @{@"identifier":info[(__bridge NSString *)kSecCodeInfoIdentifier],
        @"cdhash":hex_data(info[(__bridge NSString *)kSecCodeInfoUnique]),
        @"kind":@"ad-hoc",
        @"entitlements":info[(__bridge NSString *)kSecCodeInfoEntitlementsDict]
            ?: @{}};
    return result;
}

static BOOL
is_system_path(NSString *path)
{
    return [path isEqual:[path stringByStandardizingPath]] &&
        ([path hasPrefix:@"/usr/lib/"] ||
         [path hasPrefix:@"/System/Library/"]);
}

/* Inspect all Mach-O slices. Load commands, not the builder's search path,
 * determine runtime dependencies and deployment requirements. */
static NSDictionary *
audit_macho(NSString *path)
{
    NSString *output;
    NSString *line;
    NSString *trimmed;
    NSString *command;
    NSString *minimum;
    NSString *value;
    NSArray *words;
    NSArray *architectures;
    NSUInteger position;
    BOOL platform_ok;
    output = run(@"/usr/bin/xcrun", @[@"lipo", @"-archs", path], NULL);
    architectures = [[output stringByTrimmingCharactersInSet:
        [NSCharacterSet whitespaceAndNewlineCharacterSet]]
        componentsSeparatedByString:@" "];
    require(architectures.count > 0, @"Missing Mach-O architecture");
    for (value in architectures) {
        require([@[@"arm64", @"x86_64"] containsObject:value],
                @"Unsupported package architecture");
    }
    output = run(@"/usr/bin/xcrun", @[@"otool", @"-L", path], NULL);
    for (line in [output componentsSeparatedByString:@"\n"]) {
        if (![line hasPrefix:@"\t"]) {
            continue;
        }
        trimmed = [line stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceCharacterSet]];
        position = [trimmed rangeOfString:@" ("].location;
        require(position != NSNotFound, @"Unrecognized Mach-O dependency");
        value = [trimmed substringToIndex:position];
        require(is_system_path(value),
            [@"Non-system runtime dependency: "
                stringByAppendingString:trimmed]);
    }
    output = run(@"/usr/bin/xcrun", @[@"otool", @"-l", path], NULL);
    command = @"";
    minimum = @"";
    platform_ok = NO;
    for (line in [output componentsSeparatedByString:@"\n"]) {
        words = [[line componentsSeparatedByCharactersInSet:
            [NSCharacterSet whitespaceCharacterSet]]
            filteredArrayUsingPredicate:[NSPredicate
                predicateWithFormat:@"length > 0"]];
        if (words.count < 2) {
            continue;
        }
        if ([words[0] isEqual:@"cmd"]) {
            command = words[1];
        }
        if ([command isEqual:@"LC_RPATH"] &&
            [words[0] isEqual:@"path"]) {
            require(words.count == 4 && is_system_path(words[1]),
                @"Non-system LC_RPATH is not distributable");
        }
        if ([command isEqual:@"LC_BUILD_VERSION"] &&
            [words[0] isEqual:@"platform"]) {
            require([@[@"1", @"MACOS", @"macos"] containsObject:words[1]],
                    @"Non-macOS Mach-O platform");
            platform_ok = YES;
        }
        if (([command isEqual:@"LC_BUILD_VERSION"] &&
             [words[0] isEqual:@"minos"]) ||
            ([command isEqual:@"LC_VERSION_MIN_MACOSX"] &&
             [words[0] isEqual:@"version"])) {
            value = words[1];
            position = [value rangeOfCharacterFromSet:[[NSCharacterSet
                characterSetWithCharactersInString:@"0123456789."]
                invertedSet]].location;
            require(position == NSNotFound, @"Invalid Mach-O minimum OS");
            if ([command isEqual:@"LC_VERSION_MIN_MACOSX"]) {
                platform_ok = YES;
            }
            if (compare_versions(value, minimum) == NSOrderedDescending) {
                minimum = value;
            }
        }
    }
    require(platform_ok && minimum.length > 0,
            @"Missing macOS deployment load command");
    return @{@"architectures":[architectures sortedArrayUsingSelector:
        @selector(compare:)], @"minimumOS":minimum};
}

/* The pure state check also accepts a private root and supplied receipt for
 * tests. The Installer entry below fixes the root and queries the real receipt.
 * This check never repairs, registers, deletes or installs any payload. */
static void
check_state(NSString *root, NSDictionary *incoming, NSDictionary *receipt)
{
    NSString *owned;
    NSString *path;
    NSDictionary *old;
    NSComparisonResult order;
    validate_manifest(incoming);
    for (owned in owned_roots()) {
        check_ancestors(root, owned);
    }
    if (receipt.count == 0) {
        for (owned in owned_roots()) {
            path = [root stringByAppendingPathComponent:owned];
            require(![[NSFileManager defaultManager] fileExistsAtPath:path],
                    @"Unowned Quick Look destination already exists");
        }
        return;
    }
    require([receipt[@"pkgid"] isEqual:package_id] &&
        [receipt[@"volume"] isEqual:@"/"] &&
        [@[@"/", @""] containsObject:receipt[@"install-location"]],
        @"Receipt identifier or installation location mismatch");
    check_ancestors(root, [support_path
        stringByAppendingPathComponent:@"payload-manifest.json"]);
    old = read_json(manifest_path(root));
    validate_manifest(old);
    require([receipt[@"pkg-version"] isEqual:old[@"packageVersion"]],
            @"Receipt and installed manifest version mismatch");
    verify_payload(root, old);
    order = compare_versions(incoming[@"packageVersion"],
                             old[@"packageVersion"]);
    require(order != NSOrderedAscending, @"Package downgrade is refused");
    require(order != NSOrderedSame || [incoming isEqual:old],
            @"Same package version has a different payload; increase version");
}

static NSDictionary *
read_receipt(void)
{
    NSString *listed;
    NSString *info;
    NSDictionary *receipt;
    NSArray *packages;
    int status;
    /* A filtered --pkgs query returns exit 1 for no match. Use the typed
     * successful list instead of treating a query error as absence. */
    listed = run(SIXEL_PKGUTIL, @[@"--pkgs-plist"], NULL);
    packages = [NSPropertyListSerialization propertyListWithData:
        [listed dataUsingEncoding:NSUTF8StringEncoding] options:0
        format:NULL error:nil];
    require([packages isKindOfClass:[NSArray class]],
            @"Cannot enumerate package receipts");
    receipt = @{};
    if ([packages containsObject:package_id]) {
        info = run(SIXEL_PKGUTIL, @[@"--pkg-info-plist", package_id],
                   &status);
        require(status == 0, @"Cannot read existing package receipt");
        receipt = [NSPropertyListSerialization propertyListWithData:
            [info dataUsingEncoding:NSUTF8StringEncoding] options:0
            format:NULL error:nil];
        require([receipt isKindOfClass:[NSDictionary class]],
                @"Invalid package receipt");
    }
    return receipt;
}

static void
preflight(NSString *target, NSString *incoming_path)
{
    require([target isEqual:@"/"], @"Only the system volume / is supported");
    check_state(@"/", read_json(incoming_path), read_receipt());
}

#include "SixelInstall.h"
#include "SixelInstalledChecks.h"
#include "SixelUser.h"
#include "SixelLegacy.h"
#include "SixelLifecycle.h"

static NSDictionary *
component(NSString *path)
{
    return @{@"RootRelativeBundlePath":path,
        @"BundleIsRelocatable":@NO, @"BundleHasStrictIdentifier":@YES,
        @"BundleIsVersionChecked":@YES, @"BundleOverwriteAction":@"upgrade"};
}

static NSArray *
xml_values(NSXMLDocument *document, NSString *xpath)
{
    NSArray *nodes;
    NSXMLNode *node;
    NSMutableArray *values;
    nodes = [document nodesForXPath:xpath error:nil];
    require(nodes != nil, @"Cannot query package XML");
    values = [NSMutableArray array];
    for (node in nodes) {
        [values addObject:node.stringValue];
    }
    return values;
}

static NSString *
xml_value(NSXMLDocument *document, NSString *xpath)
{
    NSArray *values;
    values = xml_values(document, xpath);
    require(values.count == 1, @"Missing or duplicated package XML value");
    return values[0];
}

static void
print_xml_value(NSString *path, NSString *xpath)
{
    NSXMLDocument *document;
    document = [[NSXMLDocument alloc] initWithData:read_data(path)
        options:NSXMLNodeLoadExternalEntitiesNever error:nil];
    require(document != nil, @"Invalid package XML");
    printf("%s\n", xml_value(document, xpath).UTF8String);
}

/* pkgbuild has no public switch for this PackageInfo policy. Expand the
 * container without unpacking Payload, set the directory preservation flag,
 * then reflatten with Apple's tool. Signed payload and Bom stay opaque and
 * byte-identical; pkgutil handles Scripts expansion/repacking. This is
 * package authoring, never an installation script. */
static void
preserve_directory_permissions(NSString *work)
{
    NSString *component;
    NSString *path;
    NSXMLDocument *info;
    NSXMLNode *attribute;
    component = [work stringByAppendingPathComponent:@"component"];
    run(@"/usr/sbin/pkgutil", @[@"--expand",
        [work stringByAppendingPathComponent:@"raw-payload.pkg"], component],
        NULL);
    path = [component stringByAppendingPathComponent:@"PackageInfo"];
    info = [[NSXMLDocument alloc] initWithData:read_data(path)
        options:NSXMLNodeLoadExternalEntitiesNever error:nil];
    require(info != nil && [info.rootElement.name isEqual:@"pkg-info"],
            @"Invalid component package metadata");
    attribute = [info.rootElement attributeForName:@"overwrite-permissions"];
    require(attribute != nil, @"Missing directory permissions policy");
    attribute.stringValue = @"false";
    write_data(info.XMLData, path);
    run(@"/usr/sbin/pkgutil", @[@"--flatten", component,
        [work stringByAppendingPathComponent:@"payload.pkg"]], NULL);
}

/* Bom includes shared parent placeholders, but overwrite-permissions=false
 * prevents their recorded defaults from replacing existing directory attrs.
 * All real payload entries must retain the declared mode/type and root:wheel
 * ownership. Apple's optional AppleDouble entries must name a known sibling
 * and carry exactly its attributes; they do not authorize extra payload. */
static void
check_bom(NSString *path, NSDictionary *manifest)
{
    NSMutableDictionary *expected;
    NSMutableSet *seen;
    NSMutableSet *raw_seen;
    NSDictionary *entry;
    NSString *relative;
    NSString *canonical;
    NSString *leaf;
    NSString *line;
    NSString *listing;
    NSArray *fields;
    unsigned int mode;
    validate_manifest(manifest);
    expected = [NSMutableDictionary dictionary];
    for (relative in @[@".", @"Applications", @"Library",
        @"Library/Application Support",
        @"Library/Application Support/libsixel"]) {
        expected[relative] = @"40755";
    }
    for (entry in manifest[@"files"]) {
        mode = [entry[@"mode"] unsignedIntValue];
        if ([entry[@"kind"] isEqual:@"directory"]) {
            require(mode == 0755, @"Owned directory mode must be 0755");
            mode |= S_IFDIR;
        } else {
            require([entry[@"kind"] isEqual:@"file"],
                    @"Invalid manifest file type");
            mode |= S_IFREG;
        }
        expected[entry[@"path"]] = [NSString stringWithFormat:@"%o", mode];
    }
    expected[[support_path stringByAppendingPathComponent:
        @"payload-manifest.json"]] = @"100644";
    seen = [NSMutableSet set];
    raw_seen = [NSMutableSet set];
    listing = run(@"/usr/bin/lsbom", @[@"-p", @"fmug", path], NULL);
    for (line in [listing componentsSeparatedByString:@"\n"]) {
        if (line.length == 0) {
            continue;
        }
        fields = [line componentsSeparatedByString:@"\t"];
        require(fields.count == 4, @"Invalid Bom listing");
        relative = fields[0];
        if ([relative hasPrefix:@"./"]) {
            relative = [relative substringFromIndex:2];
        }
        require(![raw_seen containsObject:relative], @"Duplicate Bom entry");
        [raw_seen addObject:relative];
        canonical = relative;
        if (expected[canonical] == nil) {
            leaf = relative.lastPathComponent;
            require([leaf hasPrefix:@"._"], @"Unexpected Bom path");
            canonical = [relative.stringByDeletingLastPathComponent
                stringByAppendingPathComponent:[leaf substringFromIndex:2]];
        }
        require(expected[canonical] != nil &&
            [fields[1] isEqual:expected[canonical]] &&
            [fields[2] isEqual:@"0"] && [fields[3] isEqual:@"0"],
            @"Bom path, type, mode or ownership mismatch");
        if ([canonical isEqual:relative]) {
            [seen addObject:relative];
        }
    }
    require([seen isEqual:[NSSet setWithArray:expected.allKeys]],
            @"Missing Bom payload or parent entry");
}

/* Inspect the expanded real archive, not just the producer's XML templates. */
static void
check_metadata(NSString *expanded, NSDictionary *manifest)
{
    NSXMLDocument *info;
    NSXMLDocument *distribution;
    NSString *field;
    NSString *root;
    NSString *path;
    NSDictionary *code;
    NSDictionary *plist;
    NSArray *identifiers;
    NSSet *expected;
    validate_manifest(manifest);
    info = [[NSXMLDocument alloc] initWithData:read_data([expanded
        stringByAppendingPathComponent:@"payload.pkg/PackageInfo"])
        options:NSXMLNodeLoadExternalEntitiesNever error:nil];
    distribution = [[NSXMLDocument alloc] initWithData:read_data([expanded
        stringByAppendingPathComponent:@"Distribution"])
        options:NSXMLNodeLoadExternalEntitiesNever error:nil];
    require(info != nil && distribution != nil, @"Invalid package XML");
    require([xml_value(info, @"/pkg-info/@identifier") isEqual:package_id] &&
        [xml_value(info, @"/pkg-info/@version")
            isEqual:manifest[@"packageVersion"]] &&
        [xml_value(info, @"/pkg-info/@install-location") isEqual:@"/"] &&
        [xml_value(info, @"/pkg-info/@auth") isEqual:@"root"] &&
        [xml_value(info, @"/pkg-info/@relocatable") isEqual:@"false"],
        @"Package identity, version or fixed location mismatch");
    require([xml_value(info, @"/pkg-info/@overwrite-permissions")
        isEqual:@"false"], @"Existing directory attributes must be preserved");
    check_bom([expanded stringByAppendingPathComponent:@"payload.pkg/Bom"],
              manifest);
    expected = [NSSet setWithArray:
        @[@"com.saitoha.libsixel.quicklook.preview",
          @"com.saitoha.libsixel.quicklook.thumbnail",
          @"com.saitoha.libsixel.quicklook.host",
          @"com.saitoha.libsixel.quicklook.bridge"]];
    for (field in @[@"bundle-version", @"upgrade-bundle",
                    @"strict-identifier"]) {
        identifiers = xml_values(info, [NSString stringWithFormat:
            @"/pkg-info/%@/bundle/@id", field]);
        require(identifiers.count == 4 &&
            [[NSSet setWithArray:identifiers] isEqual:expected],
            @"Package component policy mismatch");
    }
    require(xml_values(info, @"/pkg-info/relocate/*").count == 0,
            @"Package relocation is not allowed");
    require([xml_value(info, @"/pkg-info/scripts/preinstall/@file")
        isEqual:@"./preinstall"], @"Missing package preflight script");
    require([xml_value(info, @"/pkg-info/scripts/postinstall/@file")
        isEqual:@"./postinstall"], @"Missing package lock completion script");
    for (field in @[@"enable_anywhere", @"enable_currentUserHome"]) {
        require([xml_value(distribution, [NSString stringWithFormat:
            @"/installer-gui-script/domains/@%@", field]) isEqual:@"false"],
            @"Unsupported Installer domain");
    }
    require([xml_value(distribution,
        @"/installer-gui-script/domains/@enable_localSystem") isEqual:@"true"],
        @"System Installer domain is required");
    require([xml_value(distribution, @"//allowed-os-versions/os-version/@min")
        isEqual:manifest[@"minimumOS"]] &&
        [xml_value(distribution, @"/installer-gui-script/options/"
            @"@hostArchitectures") isEqual:[manifest[@"architectures"]
                componentsJoinedByString:@","]],
        @"Package deployment requirements mismatch");
    require([xml_value(distribution,
        @"/installer-gui-script/pkg-ref[@version]/@version")
        isEqual:manifest[@"packageVersion"]], @"Product version mismatch");
    root = [expanded stringByAppendingPathComponent:@"payload.pkg/Payload"];
    for (code in manifest[@"codes"]) {
        if (code[@"bundleIdentifier"] == nil) {
            continue;
        }
        path = [[root stringByAppendingPathComponent:code[@"path"]]
            stringByAppendingPathComponent:@"Contents/Info.plist"];
        plist = [NSDictionary dictionaryWithContentsOfFile:path];
        require([plist[@"CFBundleIdentifier"] isEqual:code[@"bundleIdentifier"]]
            && [plist[@"CFBundleVersion"] isEqual:manifest[@"packageVersion"]]
            && [plist[@"CFBundleShortVersionString"]
                isEqual:manifest[@"releaseVersion"]]
            && [plist[@"LSMinimumSystemVersion"]
                isEqual:manifest[@"minimumOS"]],
            @"Packaged bundle metadata differs from the distribution");
    }
    require([hash_file([expanded stringByAppendingPathComponent:
        @"payload.pkg/Scripts/sixel-quicklook-package"])
        isEqual:hash_file([[root stringByAppendingPathComponent:support_path]
            stringByAppendingPathComponent:@"sixel-quicklook-package"])],
        @"Package preflight executable differs from payload");
    require([hash_file([expanded stringByAppendingPathComponent:
        @"payload.pkg/Scripts/payload-manifest.json"])
        isEqual:hash_file(manifest_path(root))],
        @"Package preflight manifest differs from payload");
}

#include "SixelPackageChecks.h"

static void
build_package(NSArray *args)
{
    NSFileManager *fm;
    NSString *output;
    NSString *work;
    NSString *root;
    NSString *scripts;
    NSString *path;
    NSString *relative;
    NSString *binary;
    NSString *minimum;
    NSString *release;
    NSString *version;
    NSString *source_version;
    NSString *distribution;
    NSString *package;
    NSString *expanded;
    NSDictionary *audit;
    NSDictionary *identity;
    NSDictionary *manifest;
    NSMutableDictionary *plist;
    NSMutableArray *codes;
    NSMutableArray *audits;
    NSMutableArray *components;
    NSMutableArray *sign_args;
    NSArray *architectures;
    NSArray *parts;
    NSUInteger i;
    double phase_started;
    phase_started = monotonic_seconds();
    fm = [NSFileManager defaultManager];
    require(args.count == 12, @"build requires output, apps, helper, "
        @"entitlements, source version, package version, "
        @"preinstall and postinstall");
    output = args[2];
    version = args[9];
    source_version = args[8];
    version_parts(version);
    parts = [source_version componentsSeparatedByString:@"-"];
    release = parts[0];
    version_parts(release);
    require([output isAbsolutePath], @"Package output must be absolute");
    make_directory(output);
    /* Only a fresh private work directory is replaced. Failed builds leave
     * the previous published package intact; successful publication is last. */
    work = [output stringByAppendingPathComponent:@"work"];
    require(![fm fileExistsAtPath:work] ||
        [fm removeItemAtPath:work error:nil], @"Cannot clear package work");
    root = [work stringByAppendingPathComponent:@"root"];
    scripts = [work stringByAppendingPathComponent:@"scripts"];
    make_directory(scripts);
    copy_file(args[3], [root stringByAppendingPathComponent:
        @"Applications/SixelQuickLookHost.app"]);
    copy_file(args[4], [root stringByAppendingPathComponent:
        @"Applications/SixelPreviewBridge.app"]);
    copy_file(args[5], [root stringByAppendingPathComponent:
        [support_path
            stringByAppendingPathComponent:@"register_sixel_preview"]]);
    copy_file(args[6], [root stringByAppendingPathComponent:
        [support_path
            stringByAppendingPathComponent:@"sixel-quicklook-package"]]);
    /* Validate the copied tree before executing tools against any input. */
    payload_entries(root);
    audits = [NSMutableArray array];
    minimum = @"10.15";
    architectures = nil;
    i = 0;
    for (relative in bundle_paths()) {
        path = [root stringByAppendingPathComponent:relative];
        plist = [NSMutableDictionary dictionaryWithContentsOfFile:
            [path stringByAppendingPathComponent:@"Contents/Info.plist"]];
        require(plist != nil, @"Missing bundle plist");
        require([plist[@"CFBundleExecutable"] isEqual:
            @[@"SixelPreview", @"SixelThumbnail", @"SixelQuickLookHost",
              @"SixelPreviewBridge"][i]], @"Unexpected bundle executable");
        ++i;
        binary = [path stringByAppendingPathComponent:
            [@"Contents/MacOS" stringByAppendingPathComponent:
                plist[@"CFBundleExecutable"]]];
        [audits addObject:audit_macho(binary)];
    }
    for (relative in @[@"register_sixel_preview", @"sixel-quicklook-package"]) {
        binary = [[root stringByAppendingPathComponent:support_path]
            stringByAppendingPathComponent:relative];
        [audits addObject:audit_macho(binary)];
    }
    for (audit in audits) {
        require(architectures == nil ||
            [architectures isEqual:audit[@"architectures"]],
            @"All packaged code must have identical architecture sets");
        architectures = audit[@"architectures"];
        if (compare_versions(audit[@"minimumOS"], minimum) ==
            NSOrderedDescending) {
            minimum = audit[@"minimumOS"];
        }
    }
    codes = [NSMutableArray array];
    i = 0;
    for (relative in bundle_paths()) {
        path = [root stringByAppendingPathComponent:relative];
        plist = [NSMutableDictionary dictionaryWithContentsOfFile:
            [path stringByAppendingPathComponent:@"Contents/Info.plist"]];
        require([plist[@"CFBundleIdentifier"] isEqual:
            @[@"com.saitoha.libsixel.quicklook.preview",
              @"com.saitoha.libsixel.quicklook.thumbnail",
              @"com.saitoha.libsixel.quicklook.host",
              @"com.saitoha.libsixel.quicklook.bridge"][i]],
              @"Unexpected bundle identifier");
        plist[@"CFBundleVersion"] = version;
        plist[@"CFBundleShortVersionString"] = release;
        plist[@"LSMinimumSystemVersion"] = minimum;
        write_data([NSPropertyListSerialization dataWithPropertyList:plist
            format:NSPropertyListXMLFormat_v1_0 options:0 error:nil],
            [path stringByAppendingPathComponent:@"Contents/Info.plist"]);
        sign_args = [@[@"--force", @"--sign", @"-", @"--timestamp=none"]
            mutableCopy];
        if (i < 2) {
            [sign_args addObjectsFromArray:@[@"--entitlements", args[7]]];
        }
        [sign_args addObject:path];
        sign_private(sign_args);
        identity = signature(path);
        if (i < 2) {
            require([identity[@"entitlements"] isEqual:
                [NSDictionary dictionaryWithContentsOfFile:args[7]]],
                @"Signed extension entitlements differ from declaration");
        }
        [codes addObject:@{@"path":relative,
            @"bundleIdentifier":plist[@"CFBundleIdentifier"],
            @"bundleVersion":version, @"releaseVersion":release,
            @"minimumOS":minimum, @"macho":audits[i],
            @"signature":identity}];
        ++i;
    }
    for (relative in @[@"register_sixel_preview", @"sixel-quicklook-package"]) {
        path = [support_path stringByAppendingPathComponent:relative];
        binary = [root stringByAppendingPathComponent:path];
        sign_private(@[@"--force", @"--sign", @"-",
            @"--timestamp=none", binary]);
        [codes addObject:@{@"path":path, @"macho":audits[i],
            @"signature":signature(binary)}];
        ++i;
    }
    report_phase("prepare-audit-sign", &phase_started);
    manifest = @{@"format":@1, @"packageIdentifier":package_id,
        @"packageVersion":version, @"releaseVersion":release,
        @"sourceVersion":source_version, @"minimumOS":minimum,
        @"architectures":architectures, @"codes":codes,
        @"files":payload_entries(root)};
    validate_manifest(manifest);
    write_json(manifest, manifest_path(root));
    /* Atomic creation honors the caller's umask. Fix only this new staging
     * file before copying or packaging it; the Bom contract is always 0644.
     * The manifest is outside signed bundles, whose bytes remain untouched. */
    require([fm setAttributes:@{NSFilePosixPermissions:@0644}
        ofItemAtPath:manifest_path(root) error:nil],
        @"Cannot set staged manifest permissions");
    copy_file(manifest_path(root),
        [scripts stringByAppendingPathComponent:@"payload-manifest.json"]);
    copy_file([root stringByAppendingPathComponent:[support_path
        stringByAppendingPathComponent:@"sixel-quicklook-package"]],
        [scripts stringByAppendingPathComponent:@"sixel-quicklook-package"]);
    copy_file(args[10], [scripts stringByAppendingPathComponent:@"preinstall"]);
    copy_file(args[11],
        [scripts stringByAppendingPathComponent:@"postinstall"]);
    components = [NSMutableArray array];
    for (relative in bundle_paths()) {
        [components addObject:component(relative)];
    }
    write_data([NSPropertyListSerialization dataWithPropertyList:components
        format:NSPropertyListXMLFormat_v1_0 options:0 error:nil],
        [work stringByAppendingPathComponent:@"components.plist"]);
    run(@"/usr/bin/pkgbuild", @[@"--root", root, @"--identifier", package_id,
        @"--version", version, @"--install-location", @"/",
        @"--ownership", @"recommended", @"--component-plist",
        [work stringByAppendingPathComponent:@"components.plist"],
        @"--scripts", scripts,
        [work stringByAppendingPathComponent:@"raw-payload.pkg"]], NULL);
    preserve_directory_permissions(work);
    distribution = [NSString stringWithFormat:
        @"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        @"<installer-gui-script minSpecVersion=\"2\">\n"
        @"<title>libsixel Quick Look</title>\n"
        @"<options customize=\"never\" require-scripts=\"false\" "
        @"hostArchitectures=\"%@\"/>\n"
        @"<domains enable_anywhere=\"false\" "
        @"enable_currentUserHome=\"false\" enable_localSystem=\"true\"/>\n"
        @"<volume-check><allowed-os-versions><os-version min=\"%@\"/>"
        @"</allowed-os-versions></volume-check>\n"
        @"<choices-outline><line choice=\"quicklook\"/></choices-outline>\n"
        @"<choice id=\"quicklook\" visible=\"false\"><pkg-ref id=\"%@\"/>"
        @"</choice>\n<pkg-ref id=\"%@\" version=\"%@\" "
        @"onConclusion=\"none\">payload.pkg</pkg-ref>\n"
        @"</installer-gui-script>\n",
        [architectures componentsJoinedByString:@","], minimum,
        package_id, package_id, version];
    path = [work stringByAppendingPathComponent:@"Distribution.xml"];
    write_data([distribution dataUsingEncoding:NSUTF8StringEncoding], path);
    package = [work stringByAppendingPathComponent:@"SixelQuickLook.pkg"];
    run(@"/usr/bin/productbuild", @[@"--distribution", path,
        @"--package-path", work, package], NULL);
    report_phase("manifest-archive", &phase_started);
    expanded = [work stringByAppendingPathComponent:@"expanded"];
    run(@"/usr/sbin/pkgutil", @[@"--expand-full", package, expanded], NULL);
    path = [expanded stringByAppendingPathComponent:@"payload.pkg/Payload"];
    verify_payload(path, manifest);
    check_metadata(expanded, manifest);
    require([read_data(manifest_path(path)) isEqual:read_data(
        manifest_path(root))], @"Packaged manifest identity mismatch");
    for (identity in codes) {
        signature([path stringByAppendingPathComponent:identity[@"path"]]);
    }
    report_phase("expand-verify", &phase_started);
    /* Nothing mutates signed payload after this point. Keep the expanded
     * archive as a declared build artifact for read-only observations. */
    write_data(read_data(manifest_path(root)),
        [output stringByAppendingPathComponent:@"payload-manifest.json"]);
    write_json(@{@"format":@2, @"expandedEntries":expanded_entries(expanded),
        @"packageSHA256":hash_file(package),
        @"manifestSHA256":hash_file(manifest_path(root)),
        @"packageSigning":@"unsigned-development",
        @"codeSigning":@"ad-hoc", @"notarized":@NO},
        [output stringByAppendingPathComponent:@"package-evidence.json"]);
    write_data(read_data(package),
        [output stringByAppendingPathComponent:@"SixelQuickLook.pkg"]);
    report_phase("publish", &phase_started);
}

int
main(int argc, const char *argv[])
{
    NSArray *args;
    NSString *command;
    int result;
    (void)argc;
    (void)argv;
    result = 0;
    @autoreleasepool {
        @try {
            args = [[NSProcessInfo processInfo] arguments];
            require(args.count >= 2, @"Expected build, verify, signature, "
                @"check-state or preflight command");
            command = args[1];
            if ([command isEqual:@"migration"] && args.count == 6) {
                result = legacy_migration(args[2], args[3], args[4], args[5]);
            } else if ([command isEqual:@"legacy"] && args.count == 6) {
                result = legacy_operation(args[2], args[3], args[4], args[5]);
            } else if ([command isEqual:@"remove-users"] && args.count == 4) {
                result = remove_for_users(args[2], args[3]);
            } else if ([command isEqual:@"lifecycle"] && args.count == 4) {
                result = run_lifecycle(args[2], args[3]);
            } else if ([command isEqual:@"user"] && args.count == 6) {
                result = user_operation(args[2], args[3], args[4], args[5]);
            } else if ([command isEqual:@"installed"] && args.count == 5) {
                result = observe_installed(args[2], args[3], args[4]);
            } else if ([command isEqual:@"user-state"] && args.count == 2) {
                print_observation(user_state());
            } else if ([command isEqual:@"manage"] && args.count == 7) {
                result = manage_package(args[2], args[3], args[4],
                                        args[5], [args[6] isEqual:@"yes"]);
            } else if ([command isEqual:@"begin-install"] && args.count == 4) {
                begin_install(args[2], args[3]);
            } else if ([command isEqual:@"end-install"] && args.count == 4) {
                end_install(args[2], args[3]);
            } else if ([command isEqual:@"build"]) {
                build_package(args);
            } else if ([command isEqual:@"check"] &&
                       (args.count == 4 || args.count == 5)) {
                check_distribution(args[2], args[3],
                    args.count == 5 ? args[4] : nil);
            } else if ([command isEqual:@"verify"] && args.count == 4) {
                verify_payload(args[2], read_json(args[3]));
            } else if ([command isEqual:@"xml-value"] && args.count == 4) {
                print_xml_value(args[2], args[3]);
            } else if ([command isEqual:@"bom"] && args.count == 4) {
                check_bom(args[2], read_json(args[3]));
            } else if ([command isEqual:@"metadata"] && args.count == 4) {
                check_metadata(args[2], read_json(args[3]));
            } else if ([command isEqual:@"signature"] && args.count == 3) {
                signature(args[2]);
            } else if ([command isEqual:@"check-state"] && args.count == 5) {
                check_state(args[2], read_json(args[3]), read_json(args[4]));
            } else if ([command isEqual:@"preflight"] && args.count == 4) {
                preflight(args[2], args[3]);
            } else {
                require(NO, @"Invalid package tool arguments");
            }
        } @catch (NSException *exception) {
            fprintf(stderr, "Quick Look package: %s\n",
                    exception.reason.UTF8String);
            result = 1;
        }
    }
    return result;
}
