/* Read-only distribution observations. These inspect finished artifacts,
 * independently of the authoring tool's lipo/otool and XML construction.
 * No command here builds, expands, signs, installs or registers anything. */
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <libkern/OSByteOrder.h>

/* Bind the entire expansion, including metadata, Bom and scripts, to the
 * archive hashed in the same evidence record. This is provenance, not a
 * cached test result: every observation enumerates and hashes again. */
static NSArray *
expanded_entries(NSString *root)
{
    NSDirectoryEnumerator *enumerator;
    NSMutableArray *entries;
    NSArray *paths;
    NSString *relative;
    NSString *path;
    NSMutableDictionary *entry;
    struct stat st;
    check_ancestors(root, @".");
    require(lstat(root.fileSystemRepresentation, &st) == 0 &&
        S_ISDIR(st.st_mode), @"Missing expanded archive");
    enumerator = [[NSFileManager defaultManager] enumeratorAtPath:root];
    require(enumerator != nil, @"Cannot enumerate expanded archive");
    paths = [enumerator.allObjects sortedArrayUsingSelector:
        @selector(compare:)];
    entries = [NSMutableArray array];
    for (relative in paths) {
        path = [root stringByAppendingPathComponent:relative];
        require(lstat(path.fileSystemRepresentation, &st) == 0 &&
            (S_ISREG(st.st_mode) || S_ISDIR(st.st_mode)),
            @"Unsupported expanded archive node");
        entry = [@{@"path":relative, @"mode":@(st.st_mode & 07777),
            @"kind":S_ISDIR(st.st_mode) ? @"directory" : @"file"}
            mutableCopy];
        if (S_ISREG(st.st_mode)) {
            entry[@"sha256"] = hash_file(path);
        }
        [entries addObject:entry];
    }
    require(entries.count > 0, @"Empty expanded archive");
    return entries;
}

static NSArray *
check_bundle_ids(void)
{
    return @[@"com.saitoha.libsixel.quicklook.preview",
        @"com.saitoha.libsixel.quicklook.thumbnail",
        @"com.saitoha.libsixel.quicklook.host",
        @"com.saitoha.libsixel.quicklook.bridge"];
}

static NSArray *
check_executable_names(void)
{
    return @[@"SixelPreview", @"SixelThumbnail", @"SixelQuickLookHost",
             @"SixelPreviewBridge"];
}

static NSArray *
check_executable_paths(NSString *root)
{
    NSMutableArray *paths;
    NSUInteger i;
    NSString *name;
    paths = [NSMutableArray array];
    for (i = 0; i < bundle_paths().count; ++i) {
        [paths addObject:[[root stringByAppendingPathComponent:
            bundle_paths()[i]] stringByAppendingPathComponent:
            [@"Contents/MacOS" stringByAppendingPathComponent:
                check_executable_names()[i]]]];
    }
    for (name in @[@"register_sixel_preview",
                             @"sixel-quicklook-package"]) {
        [paths addObject:[[root stringByAppendingPathComponent:support_path]
            stringByAppendingPathComponent:name]];
    }
    return paths;
}

/* Bounds-check before copying: malformed load commands must fail as data,
 * never become out-of-bounds reads. Supported CPUs use little-endian 64-bit
 * Mach-O; universal containers use Apple's big-endian fat32/fat64 tables. */
static uint32_t
check_word(NSData *data, NSUInteger offset)
{
    uint32_t value;
    require(offset <= data.length && data.length - offset >= sizeof(value),
            @"Truncated Mach-O word");
    memcpy(&value, (const char *)data.bytes + offset, sizeof(value));
    return OSSwapLittleToHostInt32(value);
}

static uint64_t
check_big_word(NSData *data, NSUInteger offset, BOOL wide)
{
    uint64_t value;
    uint32_t small;
    require(offset <= data.length && data.length - offset >= (wide ? 8 : 4),
            @"Truncated universal Mach-O table");
    if (wide) {
        memcpy(&value, (const char *)data.bytes + offset, 8);
        return OSSwapBigToHostInt64(value);
    }
    memcpy(&small, (const char *)data.bytes + offset, 4);
    return OSSwapBigToHostInt32(small);
}

static NSDictionary *
check_macho_slice(NSData *data)
{
    uint32_t cpu;
    uint32_t count;
    uint32_t command;
    uint32_t size;
    uint32_t name;
    uint32_t prefix;
    uint32_t version;
    NSUInteger cursor;
    NSUInteger end;
    NSUInteger i;
    NSMutableArray *references;
    NSString *minimum;
    NSString *reference;
    const char *start;
    const char *stop;
    require(data.length >= sizeof(struct mach_header_64) &&
        check_word(data, 0) == MH_MAGIC_64, @"Unsupported Mach-O header");
    cpu = check_word(data, 4);
    require(cpu == CPU_TYPE_ARM64 || cpu == CPU_TYPE_X86_64,
            @"Unsupported Mach-O CPU");
    require(check_word(data, 12) == MH_EXECUTE,
            @"Payload code must be a Mach-O executable");
    count = check_word(data, 16);
    cursor = sizeof(struct mach_header_64);
    size = check_word(data, 20);
    require(size <= data.length - cursor, @"Truncated Mach-O commands");
    end = cursor + size;
    references = [NSMutableArray array];
    minimum = nil;
    for (i = 0; i < count; ++i) {
        require(cursor <= end && end - cursor >= 8,
                @"Missing Mach-O load command");
        command = check_word(data, cursor);
        size = check_word(data, cursor + 4);
        require(size >= 8 && size <= end - cursor && size % 8 == 0,
                @"Invalid Mach-O load command size");
        if (command == LC_BUILD_VERSION || command == LC_VERSION_MIN_MACOSX) {
            require(size >= (command == LC_BUILD_VERSION ? 24 : 16),
                    @"Truncated Mach-O deployment command");
            require(minimum == nil, @"Duplicate Mach-O deployment command");
            if (command == LC_BUILD_VERSION) {
                require(check_word(data, cursor + 8) == PLATFORM_MACOS,
                        @"Non-macOS Mach-O platform");
            }
            version = check_word(data, cursor +
                (command == LC_BUILD_VERSION ? 12 : 8));
            minimum = [NSString stringWithFormat:@"%u.%u.%u",
                version >> 16, (version >> 8) & 255, version & 255];
        }
        if (command == LC_LOAD_DYLIB || command == LC_LOAD_WEAK_DYLIB ||
            command == LC_REEXPORT_DYLIB || command == LC_LAZY_LOAD_DYLIB ||
            command == LC_LOAD_UPWARD_DYLIB || command == LC_RPATH ||
            command == LC_LOAD_DYLINKER) {
            prefix = (command == LC_RPATH || command == LC_LOAD_DYLINKER) ?
                12 : 24;
            require(size >= prefix,
                    @"Truncated Mach-O dependency command");
            name = check_word(data, cursor + 8);
            require(name >= prefix && name < size,
                    @"Invalid Mach-O dependency offset");
            start = (const char *)data.bytes + cursor + name;
            stop = memchr(start, 0, size - name);
            require(stop != NULL, @"Unterminated Mach-O dependency");
            reference = [[NSString alloc] initWithBytes:start
                length:(NSUInteger)(stop - start)
                encoding:NSUTF8StringEncoding];
            require(reference != nil, @"Invalid Mach-O dependency encoding");
            [references addObject:reference];
        }
        cursor += size;
    }
    require(cursor == end && minimum != nil,
            @"Missing deployment requirement or trailing Mach-O commands");
    return @{@"architecture":cpu == CPU_TYPE_ARM64 ? @"arm64" : @"x86_64",
             @"minimumOS":minimum, @"references":references};
}

static NSArray *
check_macho(NSString *path)
{
    NSData *data;
    NSMutableArray *slices;
    NSMutableSet *seen;
    NSDictionary *slice;
    uint32_t magic;
    uint64_t count;
    uint64_t offset;
    uint64_t size;
    NSUInteger i;
    NSUInteger entry;
    BOOL wide;
    data = read_data(path);
    magic = check_word(data, 0);
    if (magic == MH_MAGIC_64) {
        return @[check_macho_slice(data)];
    }
    require(magic == FAT_CIGAM || magic == FAT_CIGAM_64,
            @"Unsupported universal Mach-O header");
    wide = magic == FAT_CIGAM_64;
    count = check_big_word(data, 4, NO);
    require(count > 0 && count <= 2 &&
        count <= (data.length - 8) / (wide ? 32 : 20),
        @"Invalid universal Mach-O slice count");
    slices = [NSMutableArray array];
    seen = [NSMutableSet set];
    for (i = 0; i < count; ++i) {
        entry = 8 + i * (wide ? 32 : 20);
        offset = check_big_word(data, entry + 8, wide);
        size = check_big_word(data, entry + (wide ? 16 : 12), wide);
        require(offset >= 8 + count * (wide ? 32 : 20) &&
            offset <= data.length && size <= data.length - offset,
            @"Invalid universal Mach-O slice range");
        slice = check_macho_slice([data subdataWithRange:
            NSMakeRange((NSUInteger)offset, (NSUInteger)size)]);
        require(check_big_word(data, entry, NO) ==
            ([slice[@"architecture"] isEqual:@"arm64"] ?
                CPU_TYPE_ARM64 : CPU_TYPE_X86_64),
            @"Universal table and Mach-O CPU disagree");
        require(![seen containsObject:slice[@"architecture"]],
                @"Duplicate universal Mach-O CPU");
        [seen addObject:slice[@"architecture"]];
        [slices addObject:slice];
    }
    return slices;
}

/* Keep independently selectable contracts separate. The dispatcher is used
 * only by read-only checks, not by the producer's acceptance validation. */
static void
check_distribution(NSString *observation, NSString *directory,
                   NSString *subject)
{
    NSString *expanded;
    NSString *root;
    NSString *path;
    NSString *value;
    NSString *minimum;
    NSString *field;
    NSString *declared_minimum;
    NSDictionary *manifest;
    NSDictionary *evidence;
    NSDictionary *plist;
    NSDictionary *declaration;
    NSDictionary *extension;
    NSDictionary *slice;
    NSArray *values;
    NSArray *slices;
    NSArray *subjects;
    NSArray *paths;
    NSXMLDocument *info;
    NSXMLDocument *distribution;
    NSMutableSet *architectures;
    NSSet *expected;
    NSUInteger i;
    NSUInteger first;
    NSUInteger last;
    struct stat st;
    subjects = @[@"preview", @"thumbnail", @"host", @"bridge",
                 @"registration-helper", @"package-helper"];
    first = [subjects indexOfObject:subject ?: @""];
    expanded = [directory stringByAppendingPathComponent:@"work/expanded"];
    root = [expanded stringByAppendingPathComponent:@"payload.pkg/Payload"];
    if ([observation isEqual:@"correspondence"]) {
        evidence = read_json([directory stringByAppendingPathComponent:
            @"package-evidence.json"]);
        require([evidence[@"format"] isEqual:@2],
                @"Missing archive expansion evidence");
        require([hash_file([directory stringByAppendingPathComponent:
            @"SixelQuickLook.pkg"]) isEqual:evidence[@"packageSHA256"]],
            @"Published archive differs from expansion evidence");
        require([hash_file([directory stringByAppendingPathComponent:
            @"payload-manifest.json"]) isEqual:evidence[@"manifestSHA256"]],
            @"Published manifest differs from expansion evidence");
        require([expanded_entries(expanded)
            isEqual:evidence[@"expandedEntries"]],
            @"Expanded archive differs from publication evidence");
        return;
    }
    if ([observation hasPrefix:@"component-"] ||
        [observation isEqual:@"domains"]) {
        info = [[NSXMLDocument alloc] initWithData:read_data([expanded
            stringByAppendingPathComponent:@"payload.pkg/PackageInfo"])
            options:NSXMLNodeLoadExternalEntitiesNever error:nil];
        if ([observation isEqual:@"domains"]) {
            distribution = [[NSXMLDocument alloc] initWithData:read_data(
                [expanded stringByAppendingPathComponent:@"Distribution"])
                options:NSXMLNodeLoadExternalEntitiesNever error:nil];
            for (field in @[@"enable_anywhere", @"enable_currentUserHome",
                           @"enable_localSystem"]) {
                require([xml_value(distribution, [NSString stringWithFormat:
                    @"/installer-gui-script/domains/@%@", field]) isEqual:
                    [field isEqual:@"enable_localSystem"] ? @"true" : @"false"],
                    @"Distribution must select only the system domain");
            }
            return;
        }
        field = [observation substringFromIndex:10];
        require([@[@"strict-identifier", @"bundle-version", @"upgrade-bundle"]
            containsObject:field], @"Unknown component observation");
        values = xml_values(info, [NSString stringWithFormat:
            @"/pkg-info/%@/bundle/@id", field]);
        require(values.count == 4 && [[NSSet setWithArray:values]
            isEqual:[NSSet setWithArray:check_bundle_ids()]],
            @"Component policy must name each fixed bundle exactly once");
        if ([field isEqual:@"upgrade-bundle"]) {
            require(xml_values(info, @"/pkg-info/update-bundle/*").count == 0 &&
                xml_values(info,
                    @"/pkg-info/atomic-update-bundle/*").count == 0,
                @"Conflicting component overwrite policy");
        }
        return;
    }
    if ([observation hasPrefix:@"entitlements-"]) {
        i = [observation isEqual:@"entitlements-preview"] ? 0 : 1;
        require([@[@"entitlements-preview", @"entitlements-thumbnail"]
            containsObject:observation], @"Unknown entitlement observation");
        require([signature([root stringByAppendingPathComponent:
            bundle_paths()[i]])[@"entitlements"] isEqual:
            @{@"com.apple.security.app-sandbox":@YES,
              @"com.apple.security.files.user-selected.read-only":@YES}],
            @"Extension must have exactly the declared sandbox entitlements");
        return;
    }
    manifest = read_json([directory stringByAppendingPathComponent:
        @"payload-manifest.json"]);
    if ([observation isEqual:@"declared-architectures"]) {
        distribution = [[NSXMLDocument alloc] initWithData:read_data(
            [expanded stringByAppendingPathComponent:@"Distribution"])
            options:NSXMLNodeLoadExternalEntitiesNever error:nil];
        require([xml_value(distribution, @"/installer-gui-script/options/"
            @"@hostArchitectures") isEqual:[manifest[@"architectures"]
                componentsJoinedByString:@","]],
            @"Distribution architectures differ from the manifest");
        return;
    }
    if ([observation isEqual:@"architectures"] ||
        [observation isEqual:@"dependencies"] ||
        [observation isEqual:@"minimum-os"]) {
        minimum = @"10.15.0";
        expected = [NSSet setWithArray:manifest[@"architectures"]];
        paths = check_executable_paths(root);
        if (![observation isEqual:@"minimum-os"]) {
            require(first != NSNotFound, @"Expected one known code subject");
            /* Only this code subject is opened. Other subjects have their
             * own TAP files and cannot suppress this observation's result. */
            paths = @[paths[first]];
        }
        for (path in paths) {
            slices = check_macho(path);
            architectures = [NSMutableSet set];
            for (slice in slices) {
                [architectures addObject:slice[@"architecture"]];
                if (compare_versions(slice[@"minimumOS"], minimum) ==
                    NSOrderedDescending) {
                    minimum = slice[@"minimumOS"];
                }
                if ([observation isEqual:@"dependencies"]) {
                    for (value in slice[@"references"]) {
                        require(is_system_path(value),
                                @"Non-system Mach-O dependency or rpath");
                    }
                }
            }
            if ([observation isEqual:@"architectures"]) {
                require([architectures isEqual:expected],
                        @"Mach-O slices differ from declared architectures");
            }
        }
        if (![observation isEqual:@"minimum-os"]) {
            return;
        }
        /* Normalize numeric components: otool may print 26.0, the load
         * command stores 26.0.0. Both denote the same deployment floor. */
        declared_minimum = manifest[@"minimumOS"];
        if ([declared_minimum componentsSeparatedByString:@"."].count == 2) {
            declared_minimum = [declared_minimum stringByAppendingString:@".0"];
        }
        require([declared_minimum isEqual:minimum],
            @"Manifest minimum OS differs from actual Mach-O maximum");
        distribution = [[NSXMLDocument alloc] initWithData:read_data([expanded
            stringByAppendingPathComponent:@"Distribution"])
            options:NSXMLNodeLoadExternalEntitiesNever error:nil];
        require([xml_value(distribution,
            @"//allowed-os-versions/os-version/@min")
            isEqual:manifest[@"minimumOS"]],
            @"Distribution minimum OS differs from payload");
    }
    /* The deployment floor is a relation across all code and bundle
     * declarations. Every other plist observation opens exactly one bundle. */
    if ([observation isEqual:@"minimum-os"]) {
        first = 0;
        last = bundle_paths().count;
    } else {
        if ([observation isEqual:@"preview-extension"]) {
            first = 0;
        } else if ([observation isEqual:@"thumbnail-extension"]) {
            first = 1;
        }
        require(first < bundle_paths().count,
                @"Expected one known bundle subject");
        last = first + 1;
    }
    for (i = first; i < last; ++i) {
        path = [root stringByAppendingPathComponent:bundle_paths()[i]];
        plist = [NSDictionary dictionaryWithContentsOfFile:
            [path stringByAppendingPathComponent:@"Contents/Info.plist"]];
        require(plist != nil, @"Missing bundle plist");
        if ([observation isEqual:@"identities"]) {
            require([plist[@"CFBundleIdentifier"]
                isEqual:check_bundle_ids()[i]], @"Unexpected bundle identity");
        } else if ([observation isEqual:@"executables"]) {
            require([plist[@"CFBundleExecutable"]
                isEqual:check_executable_names()[i]],
                @"Unexpected bundle executable declaration");
            path = [path stringByAppendingPathComponent:
                [@"Contents/MacOS" stringByAppendingPathComponent:
                    check_executable_names()[i]]];
            require(lstat(path.fileSystemRepresentation, &st) == 0 &&
                S_ISREG(st.st_mode) && (st.st_mode & 0111) == 0111,
                @"Missing regular executable bundle code");
        } else if ([observation isEqual:@"versions"]) {
            version_parts(plist[@"CFBundleVersion"]);
            version_parts(plist[@"CFBundleShortVersionString"]);
            require([plist[@"CFBundleVersion"]
                isEqual:manifest[@"packageVersion"]] &&
                [plist[@"CFBundleShortVersionString"]
                    isEqual:manifest[@"releaseVersion"]],
                @"Bundle numeric version differs from distribution");
        } else if ([observation isEqual:@"minimum-os"]) {
            require([plist[@"LSMinimumSystemVersion"]
                isEqual:manifest[@"minimumOS"]],
                @"Bundle minimum OS differs from actual payload");
        } else if ([observation isEqual:@"content-types"]) {
            values = plist[@"UTExportedTypeDeclarations"];
            require([values isKindOfClass:[NSArray class]] && values.count == 1,
                    @"Expected one exported SIXEL content type");
            declaration = values[0];
            require([declaration[@"UTTypeIdentifier"]
                isEqual:@"com.saitoha.libsixel.sixel"] &&
                [declaration[@"UTTypeConformsTo"] isEqual:@[@"public.image"]] &&
                [declaration[@"UTTypeTagSpecification"]
                    [@"public.filename-extension"] isEqual:@[@"six", @"sixel"]],
                @"Incorrect SIXEL content type or filename extensions");
            value = declaration[@"UTTypeTagSpecification"][@"public.mime-type"];
            require([value isEqual:@"image/x-sixel"] ||
                [value isEqual:@[@"image/x-sixel"]],
                @"Incorrect SIXEL MIME type");
            if (i >= 2) {
                values = plist[@"CFBundleDocumentTypes"];
                require(values.count == 1 &&
                    [values[0][@"CFBundleTypeRole"] isEqual:@"Viewer"] &&
                    [values[0][@"LSItemContentTypes"]
                        isEqual:@[@"com.saitoha.libsixel.sixel"]],
                    @"Incorrect application SIXEL document association");
            }
        } else if ([observation isEqual:@"preview-extension"] ||
                   [observation isEqual:@"thumbnail-extension"]) {
            extension = plist[@"NSExtension"];
            require([extension[@"NSExtensionPointIdentifier"] isEqual:
                i == 0 ? @"com.apple.quicklook.preview" :
                         @"com.apple.quicklook.thumbnail"] &&
                [extension[@"NSExtensionPrincipalClass"] isEqual:
                    i == 0 ? @"SixelPreviewProvider" :
                             @"SixelThumbnailProvider"]
                && [extension[@"NSExtensionAttributes"]
                    [@"QLSupportedContentTypes"]
                        isEqual:@[@"com.saitoha.libsixel.sixel"]],
                @"Incorrect Quick Look extension declaration");
        } else {
            require(NO, @"Unknown distribution observation");
        }
    }
}
