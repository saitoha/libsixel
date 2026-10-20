/* Installation uses the finished archive; never copy or re-sign system apps.
 * This file shares manifest validation with package preflight. Test drivers
 * may replace the OS boundary at compile time; the distributed executable has
 * no environment switch for an alternate root, receipt tool or Installer. */

static NSString *
install_root(void)
{
    return SIXEL_INSTALL_ROOT;
}

static NSString *
lock_path(void)
{
    return [install_root() stringByAppendingPathComponent:
        @"private/var/run/libsixel-quicklook.lock"];
}

/* The managed guard belongs to the caller, not its package-script children.
 * Keeping it outside the package lock covers receipt commit and final checks
 * without making standalone Installer clients perform manual completion. */
static NSString *
managed_lock_path(void)
{
    return [install_root() stringByAppendingPathComponent:
        @"private/var/run/libsixel-quicklook-managed.lock"];
}

/* mkdir is the atomic exclusion boundary. A failed/crashed Installer leaves
 * its lock for operator inspection; never guess that a PID or age is stale.
 * Fail immediately (wait=0), rather than hang an unattended installation. */
static void
acquire_lock(NSString *path, NSString *identity)
{
    check_ancestors(install_root(), @"private/var/run");
    require(mkdir(path.fileSystemRepresentation, 0700) == 0,
        [NSString stringWithFormat:@"Quick Look operation locked (wait=0), "
            "or lock unavailable; inspect %@; no stale removal", path]);
    @try {
        write_data([identity dataUsingEncoding:NSUTF8StringEncoding],
            [path stringByAppendingPathComponent:@"identity"]);
    } @catch (NSException *exception) {
        rmdir(path.fileSystemRepresentation);
        @throw exception;
    }
}

static void
release_lock(NSString *path, NSString *identity)
{
    NSString *file;
    file = [path stringByAppendingPathComponent:@"identity"];
    check_ancestors(install_root(), [NSString stringWithFormat:
        @"private/var/run/%@/identity", path.lastPathComponent]);
    require([read_data(file) isEqual:[identity
        dataUsingEncoding:NSUTF8StringEncoding]], @"Lock identity mismatch");
    require(unlink(file.fileSystemRepresentation) == 0 &&
        rmdir(path.fileSystemRepresentation) == 0, @"Cannot release lock");
}

static void
begin_install(NSString *target, NSString *manifest)
{
    NSString *identity;
    require([target isEqual:@"/"], @"Only the system volume / is supported");
    require(SIXEL_INSTALL_UID == 0, @"Package installation requires root");
    identity = hash_file(manifest);
    acquire_lock(lock_path(), identity);
    @try {
        check_state(install_root(), read_json(manifest), read_receipt());
    } @catch (NSException *exception) {
        /* Keep the preflight failure even when releasing the lock fails. */
        @try { release_lock(lock_path(), identity); }
        @catch (NSException *cleanup) {
            fprintf(stderr, "Lock cleanup: %s\n", cleanup.reason.UTF8String);
        }
        @throw exception;
    }
}

/* The manifest is excluded from its own digest inventory, but installed
 * mode/ownership and its independent distribution bytes remain mandatory. */
static void
verify_installed_payload(NSString *manifest)
{
    NSDictionary *expected;
    NSDictionary *entry;
    NSString *path;
    NSArray *entries;
    struct stat attributes;
    expected = read_json(manifest);
    verify_payload(install_root(), expected);
    path = manifest_path(install_root());
    require([read_data(path) isEqual:read_data(manifest)],
            @"Installed manifest bytes differ from distribution");
    require(lstat(path.fileSystemRepresentation, &attributes) == 0 &&
        S_ISREG(attributes.st_mode) && (attributes.st_mode & 07777) == 0644,
        @"Installed manifest must be regular mode 0644");
    if ([install_root() isEqual:@"/"]) {
        entries = [expected[@"files"] arrayByAddingObject:@{@"path":
            [support_path stringByAppendingPathComponent:
                @"payload-manifest.json"]}];
        for (entry in entries) {
            path = [install_root()
                stringByAppendingPathComponent:entry[@"path"]];
            require(lstat(path.fileSystemRepresentation, &attributes) == 0 &&
                attributes.st_uid == 0 && attributes.st_gid == 0,
                @"Installed payload must be owned by root:wheel");
        }
    }
}

static void
end_install(NSString *target, NSString *manifest)
{
    NSString *identity;
    NSString *path;
    require([target isEqual:@"/"], @"Only the system volume / is supported");
    require(SIXEL_INSTALL_UID == 0, @"Package completion requires root");
    identity = hash_file(manifest);
    path = [lock_path() stringByAppendingPathComponent:@"identity"];
    check_ancestors(install_root(), @"private/var/run/libsixel-quicklook.lock/"
                    "identity");
    require([read_data(path) isEqual:[identity
        dataUsingEncoding:NSUTF8StringEncoding]], @"Lock identity mismatch");
    /* Only release the package-script lock. A managed caller independently
     * keeps its outer guard through Installer exit and receipt validation.
     * Standalone Installer success requires no extra completion command. */
    verify_installed_payload(manifest);
    release_lock(lock_path(), identity);
}

static NSArray *
stage_names(void)
{
    return @[@"SixelQuickLook.pkg", @"payload-manifest.json",
             @"package-evidence.json", @"sixel-quicklook-package"];
}

static void
check_stage(NSString *directory)
{
    NSFileManager *fm;
    NSDictionary *record;
    NSArray *names;
    NSString *name;
    NSString *path;
    struct stat attributes;
    fm = [NSFileManager defaultManager];
    names = [stage_names() arrayByAddingObject:@".quicklook-stage.json"];
    require([[NSSet setWithArray:[fm contentsOfDirectoryAtPath:directory
        error:nil]] isEqual:[NSSet setWithArray:names]],
        @"Stage contains missing or unowned files; refusing replacement");
    for (name in names) {
        path = [directory stringByAppendingPathComponent:name];
        require(lstat(path.fileSystemRepresentation, &attributes) == 0 &&
            S_ISREG(attributes.st_mode), @"Unsafe staged file");
    }
    record = read_json([directory stringByAppendingPathComponent:
        @".quicklook-stage.json"]);
    require([[NSSet setWithArray:record.allKeys]
        isEqual:[NSSet setWithArray:stage_names()]], @"Invalid stage record");
    for (name in stage_names()) {
        path = [directory stringByAppendingPathComponent:name];
        require([hash_file(path) isEqual:record[name]],
            @"Staged file changed; refusing replacement or deletion");
    }
}

static void
verify_distribution(NSString *directory)
{
    NSDictionary *evidence;
    NSString *name;
    NSString *path;
    struct stat attributes;
    for (name in @[@"SixelQuickLook.pkg", @"payload-manifest.json",
                   @"package-evidence.json"]) {
        path = [directory stringByAppendingPathComponent:name];
        require(lstat(path.fileSystemRepresentation, &attributes) == 0 &&
            S_ISREG(attributes.st_mode), @"Missing distribution regular file");
    }
    evidence = read_json([directory stringByAppendingPathComponent:
        @"package-evidence.json"]);
    require([hash_file([directory stringByAppendingPathComponent:
        @"SixelQuickLook.pkg"]) isEqual:evidence[@"packageSHA256"]] &&
        [hash_file([directory stringByAppendingPathComponent:
        @"payload-manifest.json"]) isEqual:evidence[@"manifestSHA256"]],
        @"Distribution differs from package evidence");
    validate_manifest(read_json([directory stringByAppendingPathComponent:
        @"payload-manifest.json"]));
}

static void
stage_package(NSString *action, NSString *source, NSString *datadir,
              NSString *destdir)
{
    NSFileManager *fm;
    NSString *directory;
    NSString *relative;
    NSString *name;
    NSString *path;
    NSString *from;
    NSMutableDictionary *record;
    struct stat attributes;
    fm = [NSFileManager defaultManager];
    require([destdir isAbsolutePath] && [datadir isAbsolutePath] &&
        ![[destdir pathComponents] containsObject:@".."] &&
        ![[datadir pathComponents] containsObject:@".."],
        @"DESTDIR and datadir must be absolute paths without '..'");
    directory = [[destdir stringByAppendingString:datadir]
        stringByAppendingPathComponent:@"libsixel/installers"];
    relative = [directory substringFromIndex:1];
    check_ancestors(@"/", relative);
    require(![[directory stringByStandardizingPath] isEqual:
        [source stringByStandardizingPath]], @"Stage cannot replace its input");
    if ([action isEqual:@"install"]) {
        verify_distribution(source);
    }
    if (lstat(directory.fileSystemRepresentation, &attributes) == 0) {
        require(S_ISDIR(attributes.st_mode), @"Stage is not a directory");
        check_stage(directory);
        for (name in [stage_names() arrayByAddingObject:
                     @".quicklook-stage.json"]) {
            path = [directory stringByAppendingPathComponent:name];
            require(unlink(path.fileSystemRepresentation) == 0,
                    @"Cannot remove owned stage file");
        }
        require(rmdir(directory.fileSystemRepresentation) == 0,
                @"Cannot remove empty stage directory");
    } else {
        require(errno == ENOENT, @"Cannot inspect stage directory");
    }
    if ([action isEqual:@"uninstall"]) {
        return;
    }
    make_directory(directory);
    record = [NSMutableDictionary dictionary];
    for (name in stage_names()) {
        from = [source stringByAppendingPathComponent:name];
        if ([name isEqual:@"sixel-quicklook-package"]) {
            from = [[NSProcessInfo processInfo] arguments][0];
        }
        path = [directory stringByAppendingPathComponent:name];
        copy_file(from, path);
        require(chmod(path.fileSystemRepresentation,
            [name isEqual:@"sixel-quicklook-package"] ? 0755 : 0644) == 0,
            @"Cannot set distribution staging mode");
        record[name] = hash_file(path);
    }
    write_json(record, [directory stringByAppendingPathComponent:
        @".quicklook-stage.json"]);
    puts("Quick Look: distribution staged only; no system installation");
}

static void
check_installed(NSString *manifest, NSDictionary *receipt)
{
    NSDictionary *expected;
    expected = read_json(manifest);
    require(receipt.count != 0, @"Quick Look package is not installed");
    check_state(install_root(), expected, receipt);
    verify_installed_payload(manifest);
}

/* Read-only callers must not observe a managed operation as complete before
 * its outer receipt/payload check. Package locks also expose partial failures.
 * These checks do not claim synchronization with a standalone Installer after
 * its postinstall script has returned. */
static void
require_no_lock(NSString *path)
{
    struct stat attributes;
    check_ancestors(install_root(), [@"private/var/run/"
        stringByAppendingString:path.lastPathComponent]);
    require(lstat(path.fileSystemRepresentation, &attributes) != 0 &&
        errno == ENOENT, @"Quick Look operation pending or lock unavailable");
}

static int
remove_installed(NSString *manifest, BOOL users_deactivated)
{
    NSDictionary *receipt;
    NSDictionary *expected;
    NSDictionary *entry;
    NSString *path;
    NSString *identity;
    NSString *output;
    int status;
    BOOL deleting;
    BOOL package_locked;
    status = 0;
    deleting = NO;
    package_locked = NO;
    identity = [@"uninstall:" stringByAppendingString:hash_file(manifest)];
    acquire_lock(managed_lock_path(), identity);
    @try {
        acquire_lock(lock_path(), identity);
        package_locked = YES;
        receipt = read_receipt();
        if (receipt.count == 0) {
            /* Fully absent is idempotent; unowned residual files are not. */
            check_state(install_root(), read_json(manifest), receipt);
        } else {
            require(users_deactivated,
                @"System removal requires --users-deactivated after all "
                 "affected users have explicitly restored their settings");
            check_installed(manifest, receipt);
            expected = read_json(manifest);
            /* The complete comparison above rejects unknown children before
             * any deletion. unlink/rmdir never recursively delete additions.
             * Keep receipt and a failed-operation lock on partial failure. */
            deleting = YES;
            for (entry in [expected[@"files"] reverseObjectEnumerator]) {
                path = [install_root()
                    stringByAppendingPathComponent:entry[@"path"]];
                if ([path isEqual:[install_root()
                    stringByAppendingPathComponent:support_path]]) {
                    require(unlink(manifest_path(install_root())
                        .fileSystemRepresentation) == 0,
                        @"Cannot remove installed manifest");
                }
                if ([entry[@"kind"] isEqual:@"directory"]) {
                    require(rmdir(path.fileSystemRepresentation) == 0,
                            @"Cannot remove owned directory; recovery needed");
                } else {
                    require(unlink(path.fileSystemRepresentation) == 0,
                            @"Cannot remove owned file; recovery needed");
                }
            }
            output = run(SIXEL_PKGUTIL, @[@"--forget", package_id], &status);
            fputs(output.UTF8String, stderr);
            if (status != 0) {
                fprintf(stderr, "Receipt removal failed; lock retained\n");
                return status;
            }
            require(read_receipt().count == 0,
                    @"Receipt remains after removal; recovery needed");
        }
        release_lock(lock_path(), identity);
        package_locked = NO;
        release_lock(managed_lock_path(), identity);
    } @catch (NSException *exception) {
        /* Precondition failures may release safely. If deletion started,
         * retaining a lock is deliberately conservative for operator review. */
        if (!deleting) {
            if (package_locked) {
                @try { release_lock(lock_path(), identity); }
                @catch (NSException *cleanup) {
                    fprintf(stderr, "Lock cleanup: %s\n",
                            cleanup.reason.UTF8String);
                }
            }
            @try { release_lock(managed_lock_path(), identity); }
            @catch (NSException *cleanup) {
                fprintf(stderr, "Guard cleanup: %s\n",
                        cleanup.reason.UTF8String);
            }
        }
        fprintf(stderr, "Operation incomplete; inspect lock and payload\n");
        @throw exception;
    }
    return status;
}

/* Keep the administrator guidance safe to paste even for shell metacharacters
 * in a distribution path. Actual execution always uses separate argv items. */
static NSString *
command_argument(NSString *value)
{
    NSString *character;
    for (character in @[@"\\", @"\"", @"$", @"`"]) {
        value = [value stringByReplacingOccurrencesOfString:character
            withString:[@"\\" stringByAppendingString:character]];
    }
    return [NSString stringWithFormat:@"\"%@\"", value];
}

static int
manage_package(NSString *action, NSString *directory, NSString *datadir,
               NSString *destdir, BOOL users_deactivated)
{
    NSString *manifest;
    NSString *package;
    NSString *identity;
    NSString *output;
    int status;
    require([@[@"install", @"uninstall", @"installcheck"]
        containsObject:action], @"Unknown management operation");
    require(directory.length > 0, @"Package directory must not be empty");
    if (destdir.length > 0) {
        if ([action isEqual:@"installcheck"]) {
            puts("Quick Look installcheck: SKIP STAGING (DESTDIR is set)");
            return 0;
        }
        stage_package(action, directory, datadir, destdir);
        return 0;
    }
    manifest = [directory stringByAppendingPathComponent:
        @"payload-manifest.json"];
    package = [directory stringByAppendingPathComponent:@"SixelQuickLook.pkg"];
    if ([action isEqual:@"installcheck"]) {
        require_no_lock(managed_lock_path());
        require_no_lock(lock_path());
        check_installed(manifest, read_receipt());
        puts("Quick Look: installed payload and receipt match (read-only)");
        return 0;
    }
    if (SIXEL_INSTALL_UID != 0) {
        fprintf(stderr, "Quick Look %s requires root; "
                "no elevation attempted.\n",
                action.UTF8String);
        if ([action isEqual:@"install"]) {
            fprintf(stderr, "Run as administrator: /usr/sbin/installer "
                    "-pkg %s -target /\n",
                    command_argument(package).UTF8String);
        } else {
            fprintf(stderr, "Run the explicit uninstall helper as root "
                    "after user deactivation.\n");
        }
        return 77;
    }
    if ([action isEqual:@"uninstall"]) {
        return remove_installed(manifest, users_deactivated);
    }
    verify_distribution(directory);
    /* The outer guard serializes managed callers through the complete child
     * operation and final validation. Package scripts own a separate lock.
     * Any failed/exceptional child outcome retains the guard for inspection;
     * do not infer that no mutation occurred merely from a nonzero status. */
    identity = [@"install:" stringByAppendingString:hash_file(manifest)];
    acquire_lock(managed_lock_path(), identity);
    output = run(SIXEL_INSTALLER, @[@"-pkg", package, @"-target", @"/"],
                 &status);
    fputs(output.UTF8String, stderr);
    if (status != 0) {
        fprintf(stderr, "Installer failed (%d); inspect payload, receipt and "
                "lock before retrying; no automatic repair\n", status);
        return status;
    }
    require_no_lock(lock_path());
    check_installed(manifest, read_receipt());
    release_lock(managed_lock_path(), identity);
    puts("Quick Look: system installation complete; user activation separate");
    return 0;
}
