/* Explicit legacy-copy preservation. A distribution manifest, not a bundle
 * name alone, authorizes these bytes. An archive stays discoverability-neutral
 * until an explicitly requested restore into the original empty path. */
static NSArray *
legacy_inventory(NSString *path, NSString *relative)
{
    NSMutableArray *entries;
    NSDictionary *entry;
    NSMutableDictionary *mapped;
    NSString *tail;
    entries = [NSMutableArray array];
    for (entry in payload_entries_for_roots(
        path.stringByDeletingLastPathComponent, @[path.lastPathComponent])) {
        mapped = [entry mutableCopy];
        tail = [entry[@"path"]
            substringFromIndex:path.lastPathComponent.length];
        mapped[@"path"] = [relative stringByAppendingString:tail];
        [entries addObject:mapped];
    }
    return entries;
}

static NSDictionary *
legacy_attributes(NSString *path)
{
    NSMutableDictionary *result;
    NSMutableArray *paths;
    NSDirectoryEnumerator *enumerator;
    NSString *relative;
    NSString *full;
    NSMutableDictionary *attrs;
    NSMutableData *names;
    NSMutableData *value;
    const char *name;
    ssize_t count;
    ssize_t size;
    size_t offset;
    struct stat st;
    result = [NSMutableDictionary dictionary];
    paths = [NSMutableArray arrayWithObject:@""];
    enumerator = [[NSFileManager defaultManager] enumeratorAtPath:path];
    require(enumerator != nil, @"Cannot enumerate legacy attributes");
    for (relative in enumerator) {
        [paths addObject:relative];
    }
    for (relative in paths) {
        full = relative.length ? [path stringByAppendingPathComponent:
            relative] : path;
        require(lstat(full.fileSystemRepresentation, &st) == 0 &&
            (S_ISDIR(st.st_mode) || S_ISREG(st.st_mode)) &&
            st.st_uid == getuid(), @"Legacy copy contains foreign ownership");
        attrs = [NSMutableDictionary dictionary];
        count = listxattr(full.fileSystemRepresentation, NULL, 0,
            XATTR_NOFOLLOW);
        require(count >= 0, @"Cannot list legacy extended attributes");
        names = [NSMutableData dataWithLength:(NSUInteger)count];
        require(listxattr(full.fileSystemRepresentation, names.mutableBytes,
            names.length, XATTR_NOFOLLOW) == count,
            @"Legacy extended attributes changed");
        offset = 0;
        while (offset < names.length) {
            name = (const char *)names.bytes + offset;
            size = getxattr(full.fileSystemRepresentation, name, NULL, 0, 0,
                XATTR_NOFOLLOW);
            require(size >= 0, @"Cannot read legacy extended attribute");
            value = [NSMutableData dataWithLength:(NSUInteger)size];
            require(getxattr(full.fileSystemRepresentation, name,
                value.mutableBytes, value.length, 0, XATTR_NOFOLLOW) == size,
                @"Legacy extended attribute changed");
            attrs[@(name)] = [value base64EncodedStringWithOptions:0];
            offset += strlen(name) + 1;
        }
        result[relative] = attrs;
    }
    return result;
}

#ifndef SIXEL_LEGACY_PROBE
static BOOL
legacy_running(NSString *path, NSString *identifier)
{
    NSRunningApplication *app;
    for (app in [NSRunningApplication
        runningApplicationsWithBundleIdentifier:identifier]) {
        if ([app.bundleURL.path.stringByResolvingSymlinksInPath isEqual:path]) {
            return YES;
        }
    }
    return NO;
}
#else
#include SIXEL_LEGACY_PROBE
#endif

/* ditto's ZIP representation can omit attributes on the enclosing app root.
 * The owned journal is the explicit sidecar for that root only. All nested
 * attributes must survive the archive itself. Validate the complete extraction
 * before deletion is authorized, and again before publishing a restoration. */
static NSString *
legacy_extract(NSString *archive, NSString *temporary, NSString *path,
               NSString *relative, NSArray *expected, NSDictionary *attributes,
               NSDictionary *code_signature)
{
    NSString *full;
    NSString *name;
    NSData *value;
    NSDictionary *root_attributes;
    require(mkdir(temporary.fileSystemRepresentation, 0700) == 0,
        @"Archive verification workspace occupied");
    run(@"/usr/bin/ditto", @[@"-x", @"-k", @"--rsrc", @"--extattr",
        archive, temporary], NULL);
    full = [temporary stringByAppendingPathComponent:path.lastPathComponent];
    require([legacy_inventory(full, relative) isEqual:expected],
        @"Archive extraction inventory differs");
    require([attributes[@""] isKindOfClass:[NSDictionary class]],
        @"Archive root attribute evidence is missing");
    root_attributes = legacy_attributes(full)[@""];
    for (name in attributes[@""]) {
        value = [[NSData alloc] initWithBase64EncodedString:
            attributes[@""][name] options:0];
        require(value != nil, @"Invalid archived root attribute");
        /* Leave an already equal protected OS attribute alone. */
        if ([root_attributes[name] isEqual:attributes[@""][name]]) {
            continue;
        }
        require(setxattr(full.fileSystemRepresentation, name.UTF8String,
            value.bytes, value.length, 0, XATTR_NOFOLLOW) == 0,
            @"Cannot preserve archived root attribute");
    }
    require([legacy_attributes(full) isEqual:attributes] &&
        [signature(full) isEqual:code_signature],
        @"Restored archive identity differs");
    return full;
}

static int
legacy_operation(NSString *action, NSString *directory,
                 NSString *request_file, NSString *journal_directory)
{
    NSDictionary *request;
    NSDictionary *manifest;
    NSMutableDictionary *journal;
    NSMutableArray *expected;
    NSArray *inventory;
    NSDictionary *entry;
    NSDictionary *attributes;
    NSString *name;
    NSString *path;
    NSString *relative;
    NSString *archive;
    NSString *temporary;
    NSString *full;
    int fd;
    int status;
    struct stat st;
    request = read_json(request_file);
    user_session(request);
    require([@[@"archive", @"remove", @"restore"] containsObject:action],
        @"Invalid legacy-copy operation");
    manifest = expected_installed_manifest(directory);
    name = request[@"component"];
    path = request[@"path"];
    require([@[@"host", @"bridge"] containsObject:name] &&
        [path isKindOfClass:[NSString class]] && path.isAbsolutePath &&
        [path isEqual:path.stringByResolvingSymlinksInPath] &&
        ![path isEqual:[install_root() stringByAppendingPathComponent:
            installed_paths()[name]]], @"Explicit legacy app path required");
    relative = installed_paths()[name];
    expected = [NSMutableArray array];
    for (entry in manifest[@"files"]) {
        if ([entry[@"path"] isEqual:relative] ||
            [entry[@"path"] hasPrefix:
                [relative stringByAppendingString:@"/"]]) {
            [expected addObject:entry];
        }
    }
    require(expected.count > 0, @"Legacy distribution inventory is empty");
    require(!legacy_running(path, installed_identifier(name)),
        @"Legacy app is running; ask its user to quit, never kill it");
    fd = user_journal_open(journal_directory);
    archive = [journal_directory stringByAppendingPathComponent:@"copy.zip"];
    status = 0;
    @try {
        journal = user_journal_read(fd);
        if ([action isEqual:@"archive"]) {
            require(journal == nil, @"Legacy evidence already exists");
            require([legacy_inventory(path, relative) isEqual:expected] &&
                [signature(path) isEqual:expected_code(manifest, relative)
                    [@"signature"]],
                @"Legacy bytes are not owned distribution");
            attributes = legacy_attributes(path);
            journal = [@{@"format":@1, @"kind":@"legacy-copy",
                @"uid":request[@"uid"], @"session":request[@"session"],
                @"path":path, @"component":name, @"inventory":expected,
                @"attributes":attributes, @"state":@"archive-pending",
                @"manifestSHA256":hash_file([directory
                    stringByAppendingPathComponent:@"payload-manifest.json"]),
                @"userBefore":user_state()} mutableCopy];
            user_journal_write(fd, journal);
            require(lstat(archive.fileSystemRepresentation, &st) != 0 &&
                errno == ENOENT, @"Archive path is occupied");
            run(@"/usr/bin/ditto", @[@"-c", @"-k", @"--sequesterRsrc",
                @"--keepParent", @"--rsrc", @"--extattr", path, archive], NULL);
            require(chmod(archive.fileSystemRepresentation, 0600) == 0,
                @"Cannot protect legacy archive");
            require([legacy_inventory(path, relative) isEqual:expected] &&
                [legacy_attributes(path) isEqual:attributes],
                @"Legacy copy changed during archive");
            journal[@"archiveSHA256"] = hash_file(archive);
            temporary = [journal_directory
                stringByAppendingPathComponent:@"archive-check"];
            legacy_extract(archive, temporary, path, relative, expected,
                attributes, expected_code(manifest, relative)[@"signature"]);
            require([[NSFileManager defaultManager]
                removeItemAtPath:temporary error:nil],
                @"Cannot clean verified archive workspace");
            journal[@"archiveVerified"] = @YES;
            journal[@"state"] = @"archived";
        } else {
            require(lstat(archive.fileSystemRepresentation, &st) == 0 &&
                S_ISREG(st.st_mode) && st.st_uid == getuid() &&
                (st.st_mode & 07777) == 0600 && st.st_nlink == 1,
                @"Legacy archive ownership/type/mode changed");
            require([journal[@"kind"] isEqual:@"legacy-copy"] &&
                [journal[@"uid"] isEqual:request[@"uid"]] &&
                [journal[@"session"] isEqual:request[@"session"]] &&
                [journal[@"path"] isEqual:path] &&
                [journal[@"component"] isEqual:name] &&
                [journal[@"inventory"] isEqual:expected] &&
                [journal[@"archiveSHA256"] isEqual:hash_file(archive)],
                @"Legacy archive or identity mismatch");
            if ([action isEqual:@"remove"]) {
                require([journal[@"state"] isEqual:@"archived"] &&
                    [journal[@"archiveVerified"] isEqual:@YES] &&
                    [legacy_inventory(path, relative) isEqual:expected] &&
                    [legacy_attributes(path) isEqual:journal[@"attributes"]],
                    @"Legacy copy changed; refuse removal");
                journal[@"state"] = @"remove-pending";
                user_journal_write(fd, journal);
                /* Only providers below this explicitly owned Host path are
                 * unregistered; no private LS database deletion is used. */
                if ([name isEqual:@"host"]) {
                    for (full in @[@"SixelPreview.appex",
                                   @"SixelThumbnail.appex"]) {
                        run(SIXEL_PLUGIN_TOOL, @[@"-r", [path
                            stringByAppendingPathComponent:
                            [@"Contents/PlugIns" stringByAppendingPathComponent:
                                full]]], NULL);
                    }
                }
                inventory = legacy_inventory(path, relative);
                require([inventory isEqual:expected],
                    @"Legacy copy changed before deletion");
                for (entry in [inventory reverseObjectEnumerator]) {
                    full = [path stringByAppendingString:[entry[@"path"]
                        substringFromIndex:relative.length]];
                    require(([entry[@"kind"] isEqual:@"directory"] ?
                        rmdir(full.fileSystemRepresentation) :
                        unlink(full.fileSystemRepresentation)) == 0,
                        @"Partial legacy removal; preserve archive");
                }
                journal[@"state"] = @"removed";
            } else {
                require([journal[@"state"] isEqual:@"removed"] &&
                    lstat(path.fileSystemRepresentation, &st) != 0 &&
                    errno == ENOENT, @"Original path occupied or not removed");
                temporary = [journal_directory
                    stringByAppendingPathComponent:@"restore-work"];
                journal[@"state"] = @"restore-pending";
                user_journal_write(fd, journal);
                full = legacy_extract(archive, temporary, path, relative,
                    expected, journal[@"attributes"],
                    expected_code(manifest, relative)[@"signature"]);
                /* renamex_np excludes overwrite even if the user recreates
                 * the destination after our preflight. Cross-volume restore
                 * is refused with archive and workspace retained. */
                require(renamex_np(full.fileSystemRepresentation,
                    path.fileSystemRepresentation, RENAME_EXCL) == 0,
                    @"Cannot restore into original empty path");
                require(rmdir(temporary.fileSystemRepresentation) == 0,
                    @"Restore workspace cleanup failed");
                status = user_register(path);
                require(status == 0,
                    @"Copy restored but registration failed");
                journal[@"state"] = @"restored-copy";
            }
        }
        user_journal_write(fd, journal);
        print_observation(journal);
    } @catch (NSException *exception) {
        fprintf(stderr, "Legacy operation incomplete: %s; originalStatus=%d\n",
            exception.reason.UTF8String, status);
        status = status == 0 ? 1 : status;
    } @finally {
        close(fd);
    }
    return status != 0 && ((unsigned int)status & 255U) == 0 ? 1 : status;
}

/* A narrow no-setter migration binds pre-removal archives to one explicit
 * GUI-user journal. These records authorize only known absent instances;
 * ordinary activation and duplicate rejection retain their original rules. */
static NSDictionary *
legacy_migration_binding(NSDictionary *request, NSString *state)
{
    NSMutableArray *copies;
    NSMutableArray *absent;
    NSMutableSet *components;
    NSDictionary *item;
    NSDictionary *input;
    NSDictionary *record;
    NSDictionary *manifest;
    NSDictionary *before;
    NSDictionary *original;
    NSDictionary *provider;
    NSString *component;
    NSString *path;
    NSString *archive;
    NSString *relative;
    NSString *child;
    NSString *name;
    struct stat st;
    int fd;
    copies = [NSMutableArray array];
    absent = [NSMutableArray array];
    components = [NSMutableSet set];
    before = nil;
    require([request[@"legacy"] isKindOfClass:[NSArray class]] &&
        [request[@"legacy"] count] > 0 && [request[@"legacy"] count] <= 2,
        @"Migration requires explicit legacy archive inputs");
    for (item in request[@"legacy"]) {
        input = read_json(item[@"request"]);
        require([input[@"uid"] isEqual:request[@"uid"]] &&
            [input[@"session"] isEqual:request[@"session"]],
            @"Legacy and migration user/session differ");
        fd = user_journal_open(item[@"journal"]);
        @try { record = user_journal_read(fd); }
        @finally { close(fd); }
        component = record[@"component"];
        path = record[@"path"];
        require([@[@"host", @"bridge"] containsObject:component] &&
            ![components containsObject:component] &&
            [record[@"kind"] isEqual:@"legacy-copy"] &&
            [record[@"state"] isEqual:state] &&
            [record[@"uid"] isEqual:request[@"uid"]] &&
            [record[@"session"] isEqual:request[@"session"]] &&
            [record[@"archiveVerified"] isEqual:@YES] &&
            [path isEqual:input[@"path"]] &&
            [component isEqual:input[@"component"]] &&
            [path isEqual:[path stringByResolvingSymlinksInPath]],
            @"Legacy migration identity/state mismatch");
        [components addObject:component];
        manifest = expected_installed_manifest(item[@"distribution"]);
        require([record[@"manifestSHA256"] isEqual:hash_file(
            [item[@"distribution"] stringByAppendingPathComponent:
                @"payload-manifest.json"])], @"Legacy distribution changed");
        archive = [item[@"journal"] stringByAppendingPathComponent:@"copy.zip"];
        require(lstat(archive.fileSystemRepresentation, &st) == 0 &&
            S_ISREG(st.st_mode) && st.st_uid == getuid() &&
            (st.st_mode & 07777) == 0600 && st.st_nlink == 1 &&
            [hash_file(archive) isEqual:record[@"archiveSHA256"]],
            @"Legacy archive changed");
        if ([state isEqual:@"removed"]) {
            require(lstat(path.fileSystemRepresentation, &st) != 0 &&
                errno == ENOENT, @"Legacy original path is occupied");
        } else {
            relative = installed_paths()[component];
            require([legacy_inventory(path, relative)
                isEqual:record[@"inventory"]] &&
                [legacy_attributes(path) isEqual:record[@"attributes"]] &&
                [signature(path) isEqual:
                    expected_code(manifest, relative)[@"signature"]],
                @"Restored legacy copy changed");
        }
        if (before == nil) {
            before = user_values(record[@"userBefore"]);
        }
        require([before isEqual:user_values(record[@"userBefore"])],
            @"Legacy archives have different original user observations");
        [copies addObject:@{@"component":component, @"path":path,
            @"version":manifest[@"packageVersion"],
            @"manifestSHA256":record[@"manifestSHA256"],
            @"archiveSHA256":record[@"archiveSHA256"],
            @"inventory":record[@"inventory"],
            @"attributes":record[@"attributes"],
            @"signature":expected_code(manifest,
                installed_paths()[component])[@"signature"]}];
        [absent addObject:path];
        if ([component isEqual:@"host"]) {
            for (child in @[@"SixelPreview.appex", @"SixelThumbnail.appex"]) {
                [absent addObject:[path stringByAppendingPathComponent:
                    [@"Contents/PlugIns"
                        stringByAppendingPathComponent:child]]];
            }
        }
    }
    original = before[@"default:six"];
    require([original isEqual:before[@"default:sixel"]] &&
        [original[@"uti"] isEqual:@"com.saitoha.libsixel.sixel"] &&
        [absent containsObject:original[@"application"][@"path"]] &&
        [original[@"application"][@"identifier"]
            isEqual:original[@"handler"]],
        @"Migration requires the exact archived original SIXEL handler");
    for (item in copies) {
        if ([item[@"path"] isEqual:original[@"application"][@"path"]]) {
            require([item[@"signature"]
                isEqual:original[@"application"][@"signature"]] &&
                [item[@"version"]
                    isEqual:original[@"application"][@"version"]],
                @"Original default signature differs from archive");
        }
    }
    for (name in @[@"preview", @"thumbnail"]) {
        provider = before[[@"election:" stringByAppendingString:name]];
        require([provider[@"absent"] isEqual:@YES] ||
            ([absent containsObject:provider[@"path"]] &&
             [provider[@"path"] isEqual:provider[@"canonicalPath"]] &&
             [provider[@"identifier"] isEqual:installed_identifier(name)]),
            @"Original provider is not bound to an archived Host");
    }
    return @{@"copies":copies, @"absentPaths":absent, @"before":before};
}

static void
legacy_migration_absent(NSDictionary *manifest)
{
    NSDictionary *receipt;
    receipt = read_receipt();
    require(receipt.count == 0,
        @"System receipt remains; restoration is pending");
    check_state(install_root(), manifest, receipt);
    require_no_lock(managed_lock_path());
    require_no_lock(lock_path());
}

/* A vanished own instance may no longer have a queryable signature. Its
 * exact absent path is allowed during removal, never an arbitrary new path.
 * Handler/UTI changes or a different live choice stop this no-setter path. */
static void
legacy_migration_choices(NSDictionary *journal)
{
    NSDictionary *current;
    NSDictionary *owned;
    NSString *extension;
    NSString *path;
    struct stat st;
    for (extension in @[@"six", @"sixel"]) {
        current = observed_default(extension);
        owned = journal[@"owned"]
            [[@"default:" stringByAppendingString:extension]];
        require([current[@"handler"] isEqual:owned[@"handler"]] &&
            [current[@"uti"] isEqual:owned[@"uti"]],
            @"Later default choice preserved; migration is incomplete");
        if ([current[@"application"] count] == 0) {
            continue;
        }
        path = current[@"application"][@"path"];
        require([path isEqual:owned[@"application"][@"path"]] &&
            [path isEqual:current[@"application"][@"canonicalPath"]] &&
            lstat(path.fileSystemRepresentation, &st) != 0 && errno == ENOENT,
            @"Unexpected live choice; migration is incomplete");
    }
    for (extension in @[@"preview", @"thumbnail"]) {
        require(observed_plugins(installed_identifier(extension)).count == 0,
            @"Provider choice changed after removal preparation");
    }
}

static void
legacy_migration_observe(NSDictionary *binding)
{
    NSDictionary *expected;
    NSDictionary *record;
    NSDictionary *copy;
    NSDictionary *identity;
    NSMutableArray *absent;
    NSArray *records;
    NSString *name;
    NSString *key;
    NSString *path;
    absent = [NSMutableArray array];
    for (name in @[@"host", @"bridge", @"preview", @"thumbnail"]) {
        [absent addObject:[install_root()
            stringByAppendingPathComponent:installed_paths()[name]]];
    }
    for (name in @[@"host", @"bridge"]) {
        path = nil;
        identity = nil;
        for (copy in binding[@"copies"]) {
            if ([copy[@"component"] isEqual:name]) {
                path = copy[@"path"];
                identity = copy;
            }
        }
        records = user_live_records(observed_ls(installed_identifier(name)),
            @{@"absentPaths":absent});
        require(path == nil ? records.count == 0 : records.count == 1 &&
            [records[0][@"path"] isEqual:path] &&
            [records[0][@"canonicalPath"] isEqual:path] &&
            [records[0][@"version"] isEqual:identity[@"version"]] &&
            [records[0][@"identifier"] isEqual:installed_identifier(name)],
            @"Restored LS instance is absent or ambiguous");
    }
    for (name in @[@"six", @"sixel"]) {
        expected = binding[@"before"]
            [[@"default:" stringByAppendingString:name]];
        require([observed_default(name) isEqual:expected],
            @"Original default instance has not been observed restored");
    }
    for (name in @[@"preview", @"thumbnail"]) {
        expected = binding[@"before"]
            [[@"election:" stringByAppendingString:name]];
        records = observed_plugins(installed_identifier(name));
        if ([expected[@"absent"] isEqual:@YES]) {
            require(records.count == 0, @"Unexpected restored provider");
            continue;
        }
        require(records.count == 1, @"Restored provider absent or ambiguous");
        record = records[0];
        /* UUID may be newly assigned by registration. Record its fresh value,
         * but compare the original instance path/version/election contract. */
        for (key in @[@"path", @"canonicalPath", @"identifier", @"version",
                       @"election"]) {
            require([record[key] isEqual:expected[key]],
                @"Original provider observation has not been restored");
        }
    }
}

static int
legacy_migration(NSString *action, NSString *directory,
                 NSString *request_file, NSString *journal_directory)
{
    NSDictionary *request;
    NSDictionary *manifest;
    NSDictionary *binding;
    NSDictionary *item;
    NSDictionary *expected;
    NSDictionary *current;
    NSMutableDictionary *journal;
    NSArray *records;
    NSString *name;
    NSString *output;
    NSString *manifest_hash;
    NSString *path;
    NSString *component;
    struct stat st;
    int fd;
    int status;
    request = read_json(request_file);
    user_session(request);
    require([@[@"activate", @"prepare", @"restore", @"verify"]
        containsObject:action], @"Unknown migration operation");
    require(getenv("DESTDIR") == NULL || strlen(getenv("DESTDIR")) == 0,
        @"Migration rejects staging");
    require(getenv("SIXEL_INSTALLCHECK_DESTDIR") == NULL ||
        strlen(getenv("SIXEL_INSTALLCHECK_DESTDIR")) == 0,
        @"Migration rejects staging observation");
    require_no_lock(managed_lock_path());
    require_no_lock(lock_path());
    manifest = expected_installed_manifest(directory);
    manifest_hash = hash_file([directory stringByAppendingPathComponent:
        @"payload-manifest.json"]);
    binding = legacy_migration_binding(request,
        [action isEqual:@"verify"] ? @"restored-copy" : @"removed");
    if ([@[@"activate", @"prepare"] containsObject:action]) {
        if ([action isEqual:@"activate"]) {
            for (name in @[@"six", @"sixel"]) {
                expected = binding[@"before"]
                    [[@"default:" stringByAppendingString:name]];
                current = observed_default(name);
                require([current[@"handler"]
                    isEqual:expected[@"handler"]] &&
                    [current[@"uti"] isEqual:expected[@"uti"]],
                    @"Default choice changed before migration activation");
                if ([current[@"application"] count] != 0) {
                    path = current[@"application"][@"path"];
                    if ([binding[@"absentPaths"] containsObject:path]) {
                        require(lstat(path.fileSystemRepresentation, &st) != 0
                            && errno == ENOENT,
                            @"Archived default path was reoccupied");
                    } else {
                        component = [expected[@"handler"]
                            isEqual:installed_identifier(@"host")] ?
                            @"host" : @"bridge";
                        require([path isEqual:[install_root()
                            stringByAppendingPathComponent:
                                installed_paths()[component]]] &&
                            [current[@"application"][@"signature"] isEqual:
                                expected_code(manifest,
                                    installed_paths()[component])
                                        [@"signature"]],
                            @"Another same-ID default must be preserved");
                    }
                }
            }
            for (name in @[@"preview", @"thumbnail"]) {
                require(user_live_records(observed_plugins(
                    installed_identifier(name)), binding).count == 0,
                    @"Migration activation requires unowned providers absent");
            }
        }
        return user_operation_bound([action isEqual:@"activate"] ?
            @"activate" : @"deactivate", directory, request_file,
            journal_directory, binding);
    }
    legacy_migration_absent(manifest);
    fd = user_journal_open(journal_directory);
    status = 0;
    @try {
        journal = user_journal_read(fd);
        require([journal[@"uid"] isEqual:request[@"uid"]] &&
            [journal[@"session"] isEqual:request[@"session"]] &&
            [journal[@"manifestSHA256"] isEqual:manifest_hash] &&
            [journal[@"migration"] isEqual:binding] &&
            [journal[@"defaultChanged"] isEqual:@NO],
            @"Migration journal binding changed");
        if ([action isEqual:@"restore"]) {
            require([journal[@"state"] isEqual:@"removal-ready"],
                @"Migration is not ready; inspect partial outcomes");
            legacy_migration_choices(journal);
            user_event(fd, journal, action, @"pending", 0);
            for (item in request[@"legacy"]) {
                status = legacy_operation(@"restore", item[@"distribution"],
                    item[@"request"], item[@"journal"]);
                if (status != 0) {
                    break;
                }
            }
            require(status == 0, @"Legacy restoration incomplete");
            for (name in @[@"preview", @"thumbnail"]) {
                expected = binding[@"before"]
                    [[@"election:" stringByAppendingString:name]];
                records = observed_plugins(installed_identifier(name));
                if (![expected[@"absent"] isEqual:@YES] && records.count == 0) {
                    output = run(SIXEL_PLUGIN_TOOL,
                        @[@"-a", expected[@"path"]], &status);
                    fputs(output.UTF8String, stderr);
                    require(status == 0,
                        @"Restored provider registration failed");
                }
            }
            user_event(fd, journal, action, @"restored-pending-observation", 0);
        } else {
            require([journal[@"state"]
                isEqual:@"restored-pending-observation"],
                @"Migration restoration has not finished");
            legacy_migration_observe(binding);
            journal[@"mode"] = @"observed-restored-no-setter";
            user_event(fd, journal, action, @"observed-restored-no-setter", 0);
        }
        print_observation(journal);
    } @catch (NSException *exception) {
        fprintf(stderr, "Migration incomplete: %s; originalStatus=%d\n",
            exception.reason.UTF8String, status);
        status = status == 0 ? 1 : status;
        /* A pending record is intentionally not converted to success or
         * retried automatically. Retain archives and inspect residual state. */
    } @finally {
        close(fd);
    }
    return status != 0 && ((unsigned int)status & 255U) == 0 ? 1 : status;
}
