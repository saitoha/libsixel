/* Explicit GUI-user transactions. A journal is an owned operation record,
 * never permission to overwrite a later user choice. Pending records survive
 * failure; unknown outcomes require review instead of optimistic rollback. */
#ifndef SIXEL_USER_MUTATOR
static int
user_register(NSString *path)
{
    return (int)LSRegisterURL((__bridge CFURLRef)
        [NSURL fileURLWithPath:path], true);
}

static int
user_default(NSString *path, NSString *uti)
{
    dispatch_semaphore_t completed;
    __block int status;
    completed = dispatch_semaphore_create(0);
    status = 1;
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 120000
    if (@available(macOS 12.0, *)) {
        [[NSWorkspace sharedWorkspace] setDefaultApplicationAtURL:
            [NSURL fileURLWithPath:path] toOpenContentType:
            [UTType typeWithIdentifier:uti] completionHandler:
            ^(NSError *error) {
                status = error == nil ? 0 : (int)error.code;
                if (error != nil && status == 0) {
                    status = 1;
                }
                dispatch_semaphore_signal(completed);
            }];
        /* Consent can dispatch on the main queue. Pump the run loop instead
         * of blocking that queue on a semaphore. No timeout reports success
         * while an outstanding consent operation can still change state. */
        while (dispatch_semaphore_wait(completed, DISPATCH_TIME_NOW) != 0) {
            [[NSRunLoop currentRunLoop] runUntilDate:
                [NSDate dateWithTimeIntervalSinceNow:0.05]];
        }
        return status;
    }
#endif
    return (int)LSSetDefaultRoleHandlerForContentType(
        (__bridge CFStringRef)uti, kLSRolesViewer,
        (__bridge CFStringRef)[NSDictionary dictionaryWithContentsOfFile:
        [path stringByAppendingPathComponent:@"Contents/Info.plist"]]
            [@"CFBundleIdentifier"]);
}
#else
/* Compile-time private controls replace only mutations, not journal ordering,
 * preflight, installed payload verification or conditional restoration. */
#include SIXEL_USER_MUTATOR
#endif

static void
user_session(NSDictionary *request)
{
    NSDictionary *session;
    session = observed_session();
    require([request[@"uid"] isKindOfClass:[NSNumber class]] &&
        [request[@"session"] isKindOfClass:[NSNumber class]] &&
        [request[@"uid"] unsignedIntValue] != 0 &&
        [request[@"uid"] isEqual:session[@"uid"]] &&
        [request[@"uid"] isEqual:session[@"euid"]] &&
        [request[@"uid"] isEqual:session[@"consoleUID"]] &&
        [request[@"session"] isEqual:session[@"session"]] &&
        [session[@"gui"] isEqual:@YES] &&
        [session[@"remote"] isEqual:@NO],
        @"Explicit participating UID/GUI session is required");
}

/* Open the caller's existing private directory, then retain its FD through
 * writes. flock protects cooperating operations; no lock-file unlink race.
 * No HOME/SUDO_USER lookup and no root impersonation or automatic mkdir. */
static int
user_journal_open(NSString *directory)
{
    int fd;
    struct stat st;
    require(directory.isAbsolutePath &&
        [directory isEqual:directory.stringByResolvingSymlinksInPath],
        @"Journal directory must be an explicit canonical path");
    fd = open(directory.fileSystemRepresentation,
        O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    require(fd >= 0, @"Cannot open private journal directory");
    if (fstat(fd, &st) != 0 || st.st_uid != getuid() ||
        (st.st_mode & 07777) != 0700 || flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        require(NO, @"Journal directory must be owned mode0700 and unlocked");
    }
    return fd;
}

static NSMutableDictionary *
user_journal_read(int fd)
{
    int file;
    struct stat st;
    NSMutableData *data;
    char buffer[4096];
    ssize_t size;
    id value;
    file = openat(fd, "journal.json", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (file < 0 && errno == ENOENT) {
        return nil;
    }
    require(file >= 0, @"Cannot open journal");
    @try {
        require(fstat(file, &st) == 0 && S_ISREG(st.st_mode) &&
            st.st_uid == getuid() && (st.st_mode & 07777) == 0600 &&
            st.st_nlink == 1 && st.st_size <= 4 * 1024 * 1024,
            @"Unsafe journal ownership/type/mode/size");
        data = [NSMutableData data];
        while ((size = read(file, buffer, sizeof(buffer))) > 0) {
            [data appendBytes:buffer length:(NSUInteger)size];
            require(data.length <= 4 * 1024 * 1024, @"Journal too large");
        }
        require(size == 0, @"Cannot read journal");
        value = [NSJSONSerialization JSONObjectWithData:data
            options:NSJSONReadingMutableContainers error:nil];
        require([value isKindOfClass:[NSMutableDictionary class]],
            @"Invalid journal JSON");
    } @finally {
        close(file);
    }
    return value;
}

static void
user_journal_write(int fd, NSDictionary *journal)
{
    NSData *data;
    const char *bytes;
    size_t offset;
    ssize_t size;
    int file;
    data = [NSJSONSerialization dataWithJSONObject:journal
        options:NSJSONWritingSortedKeys error:nil];
    require(data != nil, @"Cannot serialize journal");
    /* A leftover .pending is interruption evidence, never removed silently.
     * Persist the intent before dispatch and the observed result afterward. */
    file = openat(fd, "journal.pending", O_WRONLY | O_CREAT | O_EXCL |
        O_NOFOLLOW | O_CLOEXEC, 0600);
    require(file >= 0, @"Journal pending write needs operator inspection");
    @try {
        require(fchmod(file, 0600) == 0, @"Cannot protect journal");
        bytes = data.bytes;
        offset = 0;
        while (offset < data.length) {
            size = write(file, bytes + offset, data.length - offset);
            if (size < 0 && errno == EINTR) {
                continue;
            }
            require(size > 0, @"Cannot write journal");
            offset += (size_t)size;
        }
        require(fsync(file) == 0, @"Cannot sync journal");
        require(renameat(fd, "journal.pending", fd, "journal.json") == 0,
            @"Cannot publish journal");
        require(fsync(fd) == 0, @"Cannot sync journal directory");
    } @finally {
        close(file);
    }
}

/* Only a migration's exact archived, currently absent paths are tombstones.
 * An unreadable path, live duplicate or unrelated stale row is not ignored. */
static NSArray *
user_live_records(NSArray *records, NSDictionary *migration)
{
    NSMutableArray *live;
    NSDictionary *record;
    NSString *path;
    struct stat st;
    live = [NSMutableArray array];
    for (record in records) {
        path = record[@"path"];
        if ([migration[@"absentPaths"] containsObject:path]) {
            require([path isEqual:record[@"canonicalPath"]] &&
                lstat(path.fileSystemRepresentation, &st) != 0 &&
                errno == ENOENT, @"Archived instance is not certainly absent");
        } else {
            [live addObject:record];
        }
    }
    return live;
}

static void
user_instances(NSDictionary *manifest, NSDictionary *migration)
{
    NSString *name;
    NSArray *records;
    for (name in @[@"host", @"bridge", @"preview", @"thumbnail"]) {
        records = [@[@"host", @"bridge"] containsObject:name] ?
            observed_ls(installed_identifier(name)) :
            observed_plugins(installed_identifier(name));
        records = user_live_records(records, migration);
        require(records.count <= 1,
            @"Duplicate user copies; preserve them before activation");
        if (records.count == 1) {
            check_registered_path(records, name, manifest);
        }
    }
}

static NSDictionary *
user_values(NSDictionary *snapshot)
{
    return @{@"election:preview":[snapshot[@"preview-plugin"] count] == 0 ?
        @{@"absent":@YES} : snapshot[@"preview-plugin"][0],
        @"election:thumbnail":[snapshot[@"thumbnail-plugin"] count] == 0 ?
        @{@"absent":@YES} : snapshot[@"thumbnail-plugin"][0],
        @"default:six":snapshot[@"six-default"],
        @"default:sixel":snapshot[@"sixel-default"]};
}

static void
user_event(int fd, NSMutableDictionary *journal, NSString *operation,
           NSString *state, int status)
{
    NSMutableArray *events;
    events = journal[@"events"];
    [events addObject:@{@"operation":operation, @"state":state,
        @"status":@(status), @"time":@([[NSDate date] timeIntervalSince1970])}];
    journal[@"operation"] = operation;
    journal[@"state"] = state;
    journal[@"current"] = user_values(user_state());
    user_journal_write(fd, journal);
}

static void
user_restorable_default(NSDictionary *value)
{
    NSArray *candidates;
    NSDictionary *app;
    check_journal_value(value, NO);
    app = value[@"application"];
    require([value[@"handler"] isKindOfClass:[NSString class]] &&
        app.count != 0, @"No restorable original default");
    require([value[@"uti"] isEqual:@"com.saitoha.libsixel.sixel"],
        @"Shared or unknown UTI: refuse broad default mutation");
    candidates = observed_ls(value[@"handler"]);
    require(candidates.count == 1 &&
        [candidates[0][@"path"] isEqual:app[@"path"]] &&
        [candidates[0][@"canonicalPath"] isEqual:app[@"canonicalPath"]] &&
        [candidates[0][@"version"] isEqual:app[@"version"]] &&
        [candidates[0][@"identifier"] isEqual:app[@"identifier"]] &&
        [app[@"path"] isEqual:app[@"canonicalPath"]] &&
        [signature(app[@"path"]) isEqual:app[@"signature"]],
        @"Original default is absent, ambiguous or changed");
}

static int
user_operation_bound(NSString *action, NSString *directory,
                     NSString *request_file, NSString *journal_directory,
                     NSDictionary *migration)
{
    NSDictionary *request;
    NSDictionary *manifest;
    NSDictionary *snapshot;
    NSDictionary *values;
    NSDictionary *original;
    NSDictionary *current;
    NSDictionary *owned;
    NSMutableDictionary *journal;
    NSMutableDictionary *owned_values;
    NSMutableArray *created;
    NSArray *records;
    NSString *name;
    NSString *key;
    NSString *path;
    NSString *output;
    NSString *manifest_hash;
    NSString *restore_path;
    int fd;
    int status;
    int cleanup_status;
    BOOL changed;
    request = read_json(request_file);
    user_session(request);
    require([@[@"activate", @"set-default", @"deactivate"]
        containsObject:action], @"Unknown explicit user operation");
    require_no_lock(managed_lock_path());
    require_no_lock(lock_path());
    manifest = expected_installed_manifest(directory);
    path = [directory stringByAppendingPathComponent:@"payload-manifest.json"];
    check_installed(path, read_receipt());
    manifest_hash = hash_file(path);
    user_instances(manifest, migration);
    fd = user_journal_open(journal_directory);
    status = 0;
    cleanup_status = 0;
    journal = nil;
    @try {
        journal = user_journal_read(fd);
        snapshot = user_state();
        values = user_values(snapshot);
        if (journal == nil) {
            require([action isEqual:@"activate"],
                @"Activation journal required before this operation");
            journal = [@{@"format":@1, @"mode":@"activated",
                @"uid":request[@"uid"], @"session":request[@"session"],
                @"manifestSHA256":manifest_hash, @"before":values,
                @"owned":[values mutableCopy], @"events":[NSMutableArray array],
                @"createdProviders":[NSMutableArray array],
                @"state":@"prepared", @"defaultChanged":@NO,
                @"snapshotBefore":snapshot} mutableCopy];
            if (migration != nil) {
                journal[@"migration"] = migration;
                journal[@"before"] = [migration[@"before"] mutableCopy];
            }
            user_journal_write(fd, journal);
        } else {
            require([journal[@"format"] isEqual:@1] &&
                [journal[@"uid"] isEqual:request[@"uid"]] &&
                [journal[@"session"] isEqual:request[@"session"]] &&
                [journal[@"manifestSHA256"] isEqual:manifest_hash] &&
                [journal[@"events"] isKindOfClass:[NSMutableArray class]] &&
                [journal[@"owned"] isKindOfClass:[NSMutableDictionary class]],
                @"Journal identity or schema mismatch");
            require((migration == nil && journal[@"migration"] == nil) ||
                [journal[@"migration"] isEqual:migration],
                @"Use the bound migration entry for this journal");
            require(![journal[@"state"] isEqual:@"pending"],
                @"Interrupted operation: preserve journal and inspect state");
        }
        owned_values = journal[@"owned"];
        created = journal[@"createdProviders"];
        require([created isKindOfClass:[NSMutableArray class]],
            @"Invalid journal provider ownership");
        if ([action isEqual:@"activate"]) {
            require([@[@"prepared", @"active"]
                containsObject:journal[@"state"]],
                @"Journal cannot be reused after failure/deactivation");
            /* An idempotent retry must never adopt a later user choice as
             * our owned value. A successful retry leaves the journal intact;
             * refusal can append diagnostics without changing before/owned. */
            if ([journal[@"state"] isEqual:@"active"]) {
                require([values isEqual:owned_values],
                    @"User state changed since activation; preserve journal");
                print_observation(journal);
                return 0;
            }
            for (name in @[@"host", @"bridge"]) {
                path = [install_root() stringByAppendingPathComponent:
                    installed_paths()[name]];
                user_event(fd, journal, [@"register:" stringByAppendingString:
                    name], @"pending", 0);
                status = user_register(path);
                if (status != 0) {
                    break;
                }
                check_registered_path(user_live_records(
                    observed_ls(installed_identifier(name)), migration),
                    name, manifest);
                user_event(fd, journal, name, @"completed", 0);
            }
            for (name in @[@"preview", @"thumbnail"]) {
                if (status != 0) {
                    break;
                }
                records = user_live_records(
                    observed_plugins(installed_identifier(name)), migration);
                if (records.count == 0) {
                    path = [install_root() stringByAppendingPathComponent:
                        installed_paths()[name]];
                    user_event(fd, journal, [@"add:" stringByAppendingString:
                        name], @"pending", 0);
                    output = run(SIXEL_PLUGIN_TOOL, @[@"-a", path], &status);
                    fputs(output.UTF8String, stderr);
                    if (status != 0) {
                        break;
                    }
                    records = user_live_records(
                        observed_plugins(installed_identifier(name)),
                        migration);
                    require(records.count == 1, @"Provider add not observed");
                    check_registered_path(records, name, manifest);
                    [created addObject:name];
                }
                key = [@"election:" stringByAppendingString:name];
                owned_values[key] = records[0];
                /* Preserve a previous ignore/default election; activation
                 * registers paths, it does not override election policy. */
                user_event(fd, journal, name, @"completed", 0);
            }
            if (status == 0) {
                user_instances(manifest, migration);
                owned_values[@"default:six"] = observed_default(@"six");
                owned_values[@"default:sixel"] = observed_default(@"sixel");
                if (migration != nil) {
                    for (name in @[@"default:six", @"default:sixel"]) {
                        require([owned_values[name][@"handler"]
                            isEqual:migration[@"before"][name][@"handler"]] &&
                            [owned_values[name][@"uti"]
                            isEqual:migration[@"before"][name][@"uti"]],
                            @"Migration default changed during activation");
                    }
                }
                user_event(fd, journal, action, @"active", 0);
            }
        } else if ([action isEqual:@"set-default"]) {
            require(migration == nil,
                @"Migration does not authorize a default setter");
            require([journal[@"state"] isEqual:@"active"] &&
                [request[@"extensions"] isEqual:@[@"six", @"sixel"]],
                @"Default operation requires explicit SIXEL targets");
            /* Both extensions can map to one UTI. Preflight both identities
             * first, then issue one mutation for the shared exact type. */
            original = observed_default(@"six");
            require([original isEqual:observed_default(@"sixel")],
                @"Extensions resolve to different default identities");
            require([request[@"restoreDefault"] isEqual:original],
                @"Explicit restorable original choice is required");
            user_restorable_default(original);
            require(![journal[@"defaultChanged"] boolValue],
                @"Repeated default change would lose original evidence");
            journal[@"defaultBefore"] = original;
            journal[@"before"][@"default:six"] = original;
            journal[@"before"][@"default:sixel"] = original;
            path = [install_root() stringByAppendingPathComponent:
                installed_paths()[@"bridge"]];
            user_event(fd, journal, action, @"pending", 0);
            status = user_default(path, original[@"uti"]);
            if (status == 0) {
                current = observed_default(@"six");
                require([current isEqual:observed_default(@"sixel")] &&
                    [current[@"application"][@"path"] isEqual:path] &&
                    [current[@"application"][@"signature"] isEqual:
                        expected_code(manifest, installed_paths()[@"bridge"])
                            [@"signature"]], @"Default readback mismatch");
                owned_values[@"default:six"] = current;
                owned_values[@"default:sixel"] = current;
                journal[@"defaultChanged"] = @YES;
                user_event(fd, journal, action, @"active", 0);
            }
        } else {
            if (migration != nil) {
                require([journal[@"state"] isEqual:@"active"] &&
                    [journal[@"defaultChanged"] isEqual:@NO] &&
                    [values isEqual:owned_values],
                    @"Migration choice changed; retain incomplete state");
            }
            require([@[@"active", @"failed", @"deactivated"]
                containsObject:journal[@"state"]],
                @"Incomplete journal cannot authorize deactivation");
            if ([journal[@"state"] isEqual:@"deactivated"]) {
                print_observation(journal);
                return 0;
            }
            if ([journal[@"defaultChanged"] boolValue]) {
                original = journal[@"defaultBefore"];
                current = observed_default(@"six");
                owned = owned_values[@"default:six"];
                changed = ![current isEqual:owned] ||
                    ![observed_default(@"sixel") isEqual:owned];
                if (changed) {
                    journal[@"defaultRestore"] = @"later-choice-preserved";
                } else {
                    user_restorable_default(original);
                    restore_path = original[@"application"][@"path"];
                    user_event(fd, journal, @"restore-default", @"pending", 0);
                    status = user_default(restore_path, original[@"uti"]);
                    if (status == 0) {
                        require([observed_default(@"six") isEqual:original] &&
                            [observed_default(@"sixel") isEqual:original],
                            @"Restoration readback mismatch");
                        journal[@"defaultRestore"] = @"restored";
                    }
                }
            }
            for (name in [created copy]) {
                if (status != 0) {
                    break;
                }
                records = observed_plugins(installed_identifier(name));
                key = [@"election:" stringByAppendingString:name];
                require(records.count == 1 &&
                    [records[0] isEqual:owned_values[key]],
                    @"Provider changed; preserve later user state");
                check_registered_path(records, name, manifest);
                path = [install_root() stringByAppendingPathComponent:
                    installed_paths()[name]];
                user_event(fd, journal, [@"remove:" stringByAppendingString:
                    name], @"pending", 0);
                output = run(SIXEL_PLUGIN_TOOL, @[@"-r", path], &status);
                fputs(output.UTF8String, stderr);
                if (status == 0) {
                    require(observed_plugins(installed_identifier(name))
                        .count == 0, @"Provider removal not observed");
                    [created removeObject:name];
                    user_event(fd, journal, name, @"completed", 0);
                }
            }
            if (status == 0) {
                journal[@"mode"] = migration == nil ?
                    @"deactivated" : @"migration";
                journal[@"lsRemoval"] = @"await-owned-payload-removal";
                user_event(fd, journal, action, migration == nil ?
                    @"deactivated" : @"removal-ready", 0);
            }
        }
        if (status != 0) {
            journal[@"originalStatus"] = @(status);
            /* An API error can follow a side effect. Never invent ownership
             * from an error; retain pending state for explicit inspection. */
            user_event(fd, journal, action, @"pending", status);
        }
        print_observation(journal);
    } @catch (NSException *exception) {
        fprintf(stderr, "User operation incomplete: %s\n",
            exception.reason.UTF8String);
        status = status == 0 ? 1 : status;
        @try {
            if (journal != nil) {
                journal[@"originalStatus"] = @(status);
                journal[@"error"] = exception.reason;
                user_journal_write(fd, journal);
            }
        } @catch (NSException *cleanup) {
            cleanup_status = 1;
            fprintf(stderr, "Journal cleanup: %s\n", cleanup.reason.UTF8String);
        }
    } @finally {
        close(fd);
    }
    fprintf(stderr, "User operation status=%d cleanup-status=%d\n",
        status, cleanup_status);
    /* OSStatus/NSError values are wider than the POSIX exit byte. Preserve
     * the raw cause in the journal and diagnostics, never turn 256n into a
     * successful process exit. External command statuses remain unchanged. */
    return status != 0 && ((unsigned int)status & 255U) == 0 ? 1 : status;
}

static int
user_operation(NSString *action, NSString *directory,
               NSString *request_file, NSString *journal_directory)
{
    return user_operation_bound(action, directory, request_file,
        journal_directory, nil);
}

/* Root consumes explicit per-user evidence rather than impersonating GUI
 * sessions or searching home directories. The administrator still owns the
 * completeness of the participating-user list; local journals are not a
 * cryptographic attestation against their own user or later OS changes. */
static int
remove_for_users(NSString *directory, NSString *participants_file)
{
    NSDictionary *request;
    NSDictionary *participant;
    NSDictionary *journal;
    NSMutableSet *uids;
    NSString *path;
    NSString *manifest;
    NSString *hash;
    struct stat st;
    require(SIXEL_INSTALL_UID == 0, @"System removal requires root");
    request = read_json(participants_file);
    require([request[@"format"] isEqual:@1] &&
        [request[@"participants"] isKindOfClass:[NSArray class]] &&
        [request[@"participants"] count] > 0,
        @"Explicit nonempty participating-user list required");
    manifest = [directory stringByAppendingPathComponent:
        @"payload-manifest.json"];
    hash = hash_file(manifest);
    uids = [NSMutableSet set];
    for (participant in request[@"participants"]) {
        path = participant[@"journal"];
        require([participant[@"uid"] isKindOfClass:[NSNumber class]] &&
            [participant[@"uid"] unsignedIntValue] != 0 &&
            ![uids containsObject:participant[@"uid"]] &&
            [path isKindOfClass:[NSString class]] && path.isAbsolutePath &&
            [path isEqual:path.stringByResolvingSymlinksInPath],
            @"Invalid participant identity or journal path");
        [uids addObject:participant[@"uid"]];
        require(lstat(path.fileSystemRepresentation, &st) == 0 &&
            S_ISREG(st.st_mode) && st.st_nlink == 1 &&
            st.st_uid == [participant[@"uid"] unsignedIntValue] &&
            (st.st_mode & 07777) == 0600,
            @"Participant journal is not a private owned regular file");
        journal = read_json(path);
        require([journal[@"format"] isEqual:@1] &&
            [journal[@"uid"] isEqual:participant[@"uid"]] &&
            [journal[@"manifestSHA256"] isEqual:hash] &&
            (([journal[@"state"] isEqual:@"deactivated"] &&
              [journal[@"mode"] isEqual:@"deactivated"]) ||
             ([participant[@"migration"] isEqual:@YES] &&
              [participant[@"session"] isEqual:journal[@"session"]] &&
              [participant[@"journalSHA256"] isEqual:hash_file(path)] &&
              [journal[@"state"] isEqual:@"removal-ready"] &&
              [journal[@"mode"] isEqual:@"migration"] &&
              [journal[@"defaultChanged"] isEqual:@NO] &&
              [journal[@"migration"] isKindOfClass:[NSDictionary class]])) &&
            [journal[@"createdProviders"] isKindOfClass:[NSArray class]] &&
            [journal[@"createdProviders"] count] == 0 &&
            [journal[@"events"] count] > 0,
            @"Pending or unrelated user deactivation evidence");
    }
    return remove_installed(manifest, YES);
}
