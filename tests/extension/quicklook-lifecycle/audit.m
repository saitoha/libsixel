/* Independent observations of explicitly prepared lifecycle state. This
 * executable does not include the implementation under test or mutate files. */
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#include <CommonCrypto/CommonDigest.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/xattr.h>
static id
load(NSString *path)
{
    NSData *data;
    data = [NSData dataWithContentsOfFile:path];
    /* Preflight refusal intentionally has no journal or later snapshots. */
    return data == nil ? nil : [NSJSONSerialization JSONObjectWithData:
        data options:0 error:nil];
}
/* A separate oracle reads the original preparation snapshot, never the
 * product journal or the product's inventory/signature implementation. */
static NSString *
audit_hex(NSData *data)
{
    NSMutableString *text;
    const unsigned char *bytes;
    NSUInteger i;
    if (data == nil) {
        return nil;
    }
    bytes = data.bytes;
    text = [NSMutableString stringWithCapacity:data.length * 2];
    for (i = 0; i < data.length; ++i) {
        [text appendFormat:@"%02x", bytes[i]];
    }
    return text;
}

static NSDictionary *
audit_xattrs(NSString *path)
{
    NSMutableDictionary *attrs;
    NSMutableData *names;
    NSMutableData *value;
    ssize_t count;
    ssize_t size;
    size_t offset;
    const char *name;
    attrs = [NSMutableDictionary dictionary];
    count = listxattr(path.fileSystemRepresentation, NULL, 0, XATTR_NOFOLLOW);
    if (count < 0) {
        return nil;
    }
    names = [NSMutableData dataWithLength:(NSUInteger)count];
    if (listxattr(path.fileSystemRepresentation, names.mutableBytes,
                 names.length, XATTR_NOFOLLOW) != count) {
        return nil;
    }
    offset = 0;
    while (offset < names.length) {
        name = (const char *)names.bytes + offset;
        size = getxattr(path.fileSystemRepresentation, name, NULL, 0, 0,
                        XATTR_NOFOLLOW);
        if (size < 0) {
            return nil;
        }
        value = [NSMutableData dataWithLength:(NSUInteger)size];
        if (getxattr(path.fileSystemRepresentation, name, value.mutableBytes,
                     value.length, 0, XATTR_NOFOLLOW) != size) {
            return nil;
        }
        attrs[@(name)] = audit_hex(value);
        offset += strlen(name) + 1;
    }
    return attrs;
}

static BOOL
audit_signature(NSString *path, NSDictionary *expected)
{
    SecStaticCodeRef code;
    CFDictionaryRef info;
    NSDictionary *identity;
    NSDictionary *entitlements;
    BOOL ok;
    code = NULL;
    info = NULL;
    if (expected == nil || SecStaticCodeCreateWithPath((__bridge CFURLRef)
        [NSURL fileURLWithPath:path], kSecCSDefaultFlags, &code) !=
        errSecSuccess) {
        return NO;
    }
    ok = SecStaticCodeCheckValidity(code, kSecCSStrictValidate |
        kSecCSCheckAllArchitectures | kSecCSCheckNestedCode, NULL) ==
        errSecSuccess;
    if (ok && SecCodeCopySigningInformation(code, kSecCSSigningInformation,
                                           &info) == errSecSuccess) {
        identity = (__bridge NSDictionary *)info;
        entitlements = identity[(__bridge id)kSecCodeInfoEntitlementsDict];
        if (entitlements == nil) {
            entitlements = @{};
        }
        ok = [audit_hex(identity[(__bridge id)kSecCodeInfoUnique])
            isEqual:expected[@"cdhash"]] &&
            [identity[(__bridge id)kSecCodeInfoIdentifier]
                isEqual:expected[@"identifier"]] &&
            [entitlements isEqual:expected[@"entitlements"]];
    } else {
        ok = NO;
    }
    if (info != NULL) {
        CFRelease(info);
    }
    CFRelease(code);
    return ok;
}

static BOOL
audit_copy(NSString *folder, NSString *kind)
{
    NSDictionary *original;
    NSDictionary *files;
    NSDictionary *expected;
    NSString *root;
    NSString *relative;
    NSString *full;
    NSDirectoryEnumerator *enumerator;
    NSMutableSet *paths;
    NSData *data;
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    struct stat st;
    original = load([folder stringByAppendingPathComponent:
        @"original-copy.json"]);
    root = original[@"path"];
    files = original[@"files"];
    if (![root isKindOfClass:[NSString class]] || !root.isAbsolutePath ||
        ![files isKindOfClass:[NSDictionary class]] || files.count == 0 ||
        lstat(root.fileSystemRepresentation, &st) != 0 ||
        !S_ISDIR(st.st_mode)) {
        return NO;
    }
    if ([kind isEqual:@"legacy-signature"]) {
        return audit_signature(root, original[@"signature"]);
    }
    enumerator = [[NSFileManager defaultManager] enumeratorAtPath:root];
    if (enumerator == nil) {
        return NO;
    }
    paths = [NSMutableSet setWithObject:@"."];
    for (relative in enumerator) {
        [paths addObject:relative];
    }
    /* Every aspect checks the path set as well: missing/extra entries cannot
     * turn an empty comparison into a pass. Symlinks are never followed. */
    if (![paths isEqual:[NSSet setWithArray:files.allKeys]]) {
        return NO;
    }
    for (relative in paths) {
        full = [root stringByAppendingPathComponent:relative];
        expected = files[relative];
        if (lstat(full.fileSystemRepresentation, &st) != 0 ||
            !(S_ISDIR(st.st_mode) || S_ISREG(st.st_mode))) {
            return NO;
        }
        if ([kind isEqual:@"legacy-types"]) {
            if ((st.st_mode & S_IFMT) !=
                ([expected[@"mode"] unsignedIntValue] & S_IFMT)) {
                return NO;
            }
        } else if ([kind isEqual:@"legacy-modes"]) {
            if ((st.st_mode & 07777) !=
                ([expected[@"mode"] unsignedIntValue] & 07777)) {
                return NO;
            }
        } else if ([kind isEqual:@"legacy-bytes"]) {
            if (S_ISREG(st.st_mode)) {
                data = [NSData dataWithContentsOfFile:full];
                if (data == nil || data.length > UINT32_MAX) {
                    return NO;
                }
                CC_SHA256(data.bytes, (CC_LONG)data.length, digest);
                if (![audit_hex([NSData dataWithBytes:digest
                    length:sizeof(digest)]) isEqual:expected[@"sha256"]]) {
                    return NO;
                }
            } else if (expected[@"sha256"] != [NSNull null]) {
                return NO;
            }
        } else if ([kind isEqual:@"legacy-xattrs"]) {
            if (![audit_xattrs(full) isEqual:expected[@"xattrs"]]) {
                return NO;
            }
        } else {
            return NO;
        }
    }
    return YES;
}

static BOOL
audit_absent(NSString *path)
{
    struct stat st;
    return lstat(path.fileSystemRepresentation, &st) != 0 && errno == ENOENT;
}

/* Independent post-observations read the private OS endpoint and original
 * snapshots directly. No product verifier or success state substitutes for
 * file identity, provider identity or actual default resolution. */
static BOOL
audit_migration(NSString *folder, NSString *kind)
{
    NSDictionary *before;
    NSDictionary *live;
    NSDictionary *journal;
    NSDictionary *result;
    NSDictionary *expected;
    NSDictionary *actual;
    NSString *name;
    NSString *key;
    NSString *path;
    NSString *aspect;
    NSArray *records;
    before = load([folder stringByAppendingPathComponent:@"before.json"]);
    live = load([folder stringByAppendingPathComponent:@"root/live.json"]);
    journal = load([folder stringByAppendingPathComponent:
        @"journal/journal.json"]);
    if (before == nil || live == nil) {
        return NO;
    }
    if ([@[@"migration-host", @"migration-bridge"] containsObject:kind]) {
        name = [kind substringFromIndex:10];
        for (aspect in @[@"types", @"bytes", @"modes", @"xattrs",
                          @"signature"]) {
            if (!audit_copy([folder stringByAppendingPathComponent:name],
                [@"legacy-" stringByAppendingString:aspect])) {
                return NO;
            }
        }
        return YES;
    }
    if ([kind hasPrefix:@"migration-absent:"]) {
        name = [kind substringFromIndex:17];
        path = @{@"host":@"Applications/SixelQuickLookHost.app",
            @"bridge":@"Applications/SixelPreviewBridge.app",
            @"support":@"Library/Application Support/libsixel/QuickLook"}[name];
        return path != nil && audit_absent([[folder
            stringByAppendingPathComponent:@"root"]
                stringByAppendingPathComponent:path]);
    }
    if ([kind isEqual:@"migration-receipt"]) {
        return live[@"receipt"] == nil;
    }
    if ([kind hasPrefix:@"migration-provider:"]) {
        name = [kind substringFromIndex:19];
        key = [@"com.saitoha.libsixel.quicklook." stringByAppendingString:name];
        records = live[@"plugins"][key];
        if (records.count != 1 || [before[@"plugins"][key] count] != 1) {
            return NO;
        }
        expected = before[@"plugins"][key][0];
        actual = records[0];
        for (key in @[@"path", @"canonicalPath", @"version", @"identifier",
                       @"election"]) {
            if (![expected[key] isEqual:actual[key]]) {
                return NO;
            }
        }
        return YES;
    }
    if ([kind hasPrefix:@"migration-default:"]) {
        name = [kind substringFromIndex:18];
        return before[name] != nil && [live[name] isEqual:before[name]] &&
            ![live[@"calls"] containsObject:@"default"];
    }
    if ([kind isEqual:@"migration-journal"]) {
        return [journal[@"state"] isEqual:@"observed-restored-no-setter"] &&
            [journal[@"mode"] isEqual:@"observed-restored-no-setter"] &&
            [journal[@"defaultChanged"] isEqual:@NO] &&
            [journal[@"migration"][@"copies"] count] == 2;
    }
    result = load([folder
        stringByAppendingPathComponent:@"last-operation.json"]);
    if (![result[@"exit"] isEqual:@1]) {
        return NO;
    }
    if ([kind isEqual:@"migration-pending"]) {
        return [journal[@"state"] isEqual:@"restored-pending-observation"] &&
            ![live[@"six"] isEqual:before[@"six"]] &&
            ![live[@"calls"] containsObject:@"default"];
    }
    if ([kind isEqual:@"migration-refused"]) {
        return [live isEqual:load([folder stringByAppendingPathComponent:
            @"before-target.json"])];
    }
    if ([kind isEqual:@"migration-archive-refused"]) {
        return [live isEqual:load([folder stringByAppendingPathComponent:
            @"before-target.json"])] &&
            audit_copy([folder stringByAppendingPathComponent:@"bridge"],
                @"legacy-bytes") &&
            [load([folder stringByAppendingPathComponent:
                @"bridge/journal/journal.json"])[@"state"]
                    isEqual:@"archive-pending"];
    }
    return NO;
}

int
main(int argc, const char *argv[])
{
    NSString *folder;
    NSString *kind;
    NSDictionary *before;
    NSDictionary *live;
    NSDictionary *journal;
    NSDictionary *activation;
    NSDictionary *chosen;
    NSDictionary *prior_journal;
    NSDictionary *retry_journal;
    NSDictionary *prior_live;
    NSDictionary *retry_live;
    NSDictionary *result;
    NSDictionary *first_activation;
    NSDictionary *retry_activation;
    NSUInteger activation_count;
    NSData *journal_before_retry;
    NSData *journal_after_retry;
    NSArray *operations;
    NSDictionary *operation;
    id last;
    BOOL ok;
    NSArray *parts;
    NSString *field;
    NSString *root_path;
    NSString *expected_phase;
    NSDictionary *initial;
    NSDictionary *final;
    NSUInteger matches;
    @autoreleasepool {
        if (argc != 4) {
            return 2;
        }
        folder = [@(argv[1]) stringByAppendingPathComponent:@(argv[2])];
        kind = @(argv[3]);
        if ([kind hasPrefix:@"migration-"]) {
            return audit_migration(folder, kind) ? 0 : 1;
        }
        if ([kind hasPrefix:@"order-"]) {
            journal = load([folder stringByAppendingPathComponent:
                @"record/journal.json"]);
            operations = load([folder stringByAppendingPathComponent:
                @"stage-calls.json"]);
            if ([kind isEqual:@"order-normal"]) {
                ok = [operations isEqual:@[@"check", @"install", @"activate",
                    @"set-default", @"observe", @"deactivate", @"remove",
                    @"restore", @"verify-restored"]] &&
                    [journal[@"state"] isEqual:@"completed"];
            } else if ([kind isEqual:@"order-errors"]) {
                ok = [journal[@"originalStatus"] intValue] == 23 &&
                    [journal[@"cleanupStatus"] intValue] == 41 &&
                    ![operations containsObject:@"remove"];
            } else if ([kind isEqual:@"order-command-error"]) {
                result = load([folder stringByAppendingPathComponent:
                    @"result.json"]);
                ok = [result[@"exit"] intValue] == 37 &&
                    [journal[@"originalStatus"] intValue] == 37 &&
                    [journal[@"cleanupStatus"] isEqual:@0] &&
                    [operations isEqual:@[@"check", @"install", @"activate",
                        @"deactivate", @"remove", @"restore",
                        @"verify-restored"]];
            } else if ([kind isEqual:@"order-record-error"]) {
                result = load([folder stringByAppendingPathComponent:
                    @"result.json"]);
                ok = [result[@"exit"] intValue] == 37 &&
                    [result[@"stderr"] containsString:
                        @"originalStatus=37 cleanupStatus=null "
                        @"cleanupState=not-attempted recordingFailure=yes"] &&
                    [operations isEqual:@[@"check", @"install", @"activate"]] &&
                    [journal[@"state"] isEqual:@"running"] &&
                    [journal[@"stages"] count] == 3 &&
                    [[journal[@"stages"] lastObject][@"state"]
                        isEqual:@"pending"] &&
                    journal[@"originalStatus"] == nil &&
                    journal[@"cleanupStatus"] == nil;
            } else {
                ok = [operations isEqual:@[@"check", @"install"]] &&
                    [journal[@"originalStatus"] intValue] == 37;
            }
            return ok ? 0 : 1;
        }
        if ([kind isEqual:@"legacy-foreign"] ||
            [kind isEqual:@"legacy-occupied"]) {
            journal = load([folder stringByAppendingPathComponent:
                [kind isEqual:@"legacy-foreign"] ?
                    @"foreign-preserved.json" : @"occupied-preserved.json"]);
            ok = [journal[@"exit"] intValue] != 0 &&
                [journal[@"content"] isEqual:[kind isEqual:@"legacy-foreign"] ?
                    @"preserve" : @"do not replace"];
            return ok ? 0 : 1;
        }
        if ([kind isEqual:@"legacy-restored"]) {
            journal = load([folder stringByAppendingPathComponent:
                @"journal/journal.json"]);
            ok = [journal[@"state"] isEqual:@"restored-copy"] &&
                [[NSFileManager defaultManager] fileExistsAtPath:
                    journal[@"path"]] &&
                [journal[@"archiveSHA256"] length] == 64;
            return ok ? 0 : 1;
        }
        if ([@[@"legacy-types", @"legacy-bytes", @"legacy-modes",
               @"legacy-xattrs", @"legacy-signature"] containsObject:kind]) {
            return audit_copy(folder, kind) ? 0 : 1;
        }
        if ([kind hasPrefix:@"guard-"]) {
            /* The window no longer exists at observation time. Compare raw
             * before/after witnesses captured around each bounded command. */
            parts = [kind componentsSeparatedByString:@":"];
            if (parts.count != 2) {
                return 2;
            }
            journal = load([@(argv[1]) stringByAppendingPathComponent:
                [parts[0] stringByAppendingString:@".json"]]);
            field = parts[1];
            operations = journal[@"commands"];
            expected_phase = @{@"guard-1":@"postinstall-before-receipt",
                @"guard-2":@"receipt-committed-before-installer-return",
                @"guard-3":@"installer-returned-final-receipt-observation"}
                [parts[0]];
            ok = operations.count == 3 && expected_phase != nil &&
                [journal[@"phase"] isEqual:expected_phase];
            matches = 0;
            for (operation in operations) {
                initial = operation[@"before"];
                final = operation[@"after"];
                if ([@[@"uninstall", @"install", @"installcheck"]
                    containsObject:field]) {
                    if ([operation[@"action"] isEqual:field]) {
                        ++matches;
                        ok = ok && [operation[@"status"] isEqual:@1];
                    }
                } else if ([field isEqual:@"managed-lock"]) {
                    ok = ok && [initial[@"managedGuard"] isEqual:@YES] &&
                        [final[@"managedGuard"] isEqual:@YES];
                } else if ([field isEqual:@"package-lock"]) {
                    ok = ok && [initial[@"packageLock"] isEqual:@NO] &&
                        [final[@"packageLock"] isEqual:@NO];
                } else if ([field isEqual:@"receipt"]) {
                    ok = ok && initial[@"receipt"] != nil &&
                        initial[@"receipt"] != [NSNull null] &&
                        [initial[@"receipt"] isEqual:final[@"receipt"]];
                } else if ([@[@"host", @"bridge", @"support"]
                           containsObject:field]) {
                    ok = ok && [initial[@"payload"][field]
                        isKindOfClass:[NSDictionary class]] &&
                        [initial[@"payload"][field] count] > 0 &&
                        [initial[@"payload"][field]
                            isEqual:final[@"payload"][field]];
                } else {
                    ok = NO;
                }
            }
            if ([@[@"uninstall", @"install", @"installcheck"]
                containsObject:field]) {
                ok = ok && matches == 1;
            }
            return ok ? 0 : 1;
        }
        if ([kind hasPrefix:@"standalone"]) {
            journal = load([@(argv[1]) stringByAppendingPathComponent:
                @"standalone.json"]);
            root_path = [@(argv[1]) stringByAppendingPathComponent:
                @"standalone/root"];
            if (![[NSFileManager defaultManager]
                fileExistsAtPath:root_path]) {
                return 1;
            }
            if ([kind isEqual:@"standalone"]) {
                ok = [journal[@"repeated"] isEqual:@2] &&
                    [journal[@"manualCompletion"] isEqual:@NO];
            } else if ([kind isEqual:@"standalone-receipt"]) {
                ok = audit_absent([root_path.stringByDeletingLastPathComponent
                    stringByAppendingPathComponent:@"receipt.json"]);
            } else if ([kind isEqual:@"standalone-managed-lock"]) {
                ok = audit_absent([root_path stringByAppendingPathComponent:
                    @"private/var/run/libsixel-quicklook-managed.lock"]);
            } else if ([kind isEqual:@"standalone-package-lock"]) {
                ok = audit_absent([root_path stringByAppendingPathComponent:
                    @"private/var/run/libsixel-quicklook.lock"]);
            } else {
                field = @{@"standalone-host":
                    @"Applications/SixelQuickLookHost.app",
                    @"standalone-bridge":@"Applications/SixelPreviewBridge.app",
                    @"standalone-support":
                    @"Library/Application Support/libsixel/QuickLook"}[kind];
                ok = field != nil && audit_absent([root_path
                    stringByAppendingPathComponent:field]);
            }
            return ok ? 0 : 1;
        }
        before = load([folder stringByAppendingPathComponent:@"before.json"]);
        live = load([folder stringByAppendingPathComponent:@"root/live.json"]);
        journal = load([folder stringByAppendingPathComponent:
            @"journal/journal.json"]);
        activation = load([folder stringByAppendingPathComponent:
            @"after-activation.json"]);
        chosen = load([folder stringByAppendingPathComponent:
            @"after-default.json"]);
        operations = load([@(argv[1]) stringByAppendingPathComponent:
            @"operations.json"]);
        last = nil;
        for (operation in operations) {
            if ([operation[@"scenario"] isEqual:@(argv[2])]) {
                last = operation;
            }
        }
        ok = NO;
        if ([kind isEqual:@"unchanged-retry"]) {
            first_activation = nil;
            retry_activation = nil;
            activation_count = 0;
            for (operation in operations) {
                if ([operation[@"scenario"] isEqual:@"normal"] &&
                    [operation[@"action"] isEqual:@"activate"]) {
                    if (activation_count == 0) {
                        first_activation = operation;
                    } else {
                        retry_activation = operation;
                    }
                    ++activation_count;
                }
            }
            /* Preparation copies the durable file around the retry without
             * using the product journal reader. Check its bytes separately
             * from process results and the captured API mutation calls. */
            journal_before_retry = [NSData dataWithContentsOfFile:[folder
                stringByAppendingPathComponent:@"journal-before-retry.json"]];
            journal_after_retry = [NSData dataWithContentsOfFile:[folder
                stringByAppendingPathComponent:@"journal-after-retry.json"]];
            ok = journal_before_retry.length > 0 &&
                [journal_before_retry isEqual:journal_after_retry] &&
                activation_count == 2 &&
                [first_activation[@"exit"] isEqual:@0] &&
                [retry_activation[@"exit"] isEqual:@0] &&
                [first_activation[@"stdout"] length] > 0 &&
                [first_activation[@"stdout"]
                    isEqual:retry_activation[@"stdout"]] &&
                [activation[@"calls"] isEqual:
                    @[@"register", @"register", @"-a", @"-a"]];
        } else if ([kind hasPrefix:@"reactivate-"]) {
            prior_journal = load([folder stringByAppendingPathComponent:
                @"journal-before-reactivation.json"]);
            retry_journal = load([folder stringByAppendingPathComponent:
                @"journal-after-reactivation.json"]);
            prior_live = load([folder stringByAppendingPathComponent:
                @"before-reactivation.json"]);
            retry_live = load([folder stringByAppendingPathComponent:
                @"after-reactivation.json"]);
            ok = prior_journal != nil && retry_journal != nil &&
                prior_live != nil && [prior_live isEqual:retry_live] &&
                [prior_journal[@"before"] isEqual:retry_journal[@"before"]] &&
                [prior_journal[@"owned"] isEqual:retry_journal[@"owned"]] &&
                [retry_journal[@"originalStatus"] intValue] != 0;
            if ([kind isEqual:@"reactivate-default"]) {
                ok = ok && [last[@"exit"] intValue] == 0 &&
                    [live[@"six"] isEqual:prior_live[@"six"]] &&
                    [live[@"sixel"] isEqual:prior_live[@"sixel"]] &&
                    [live[@"six"][@"handler"] isEqual:
                        @"org.example.explicit-later-choice"] &&
                    [[live[@"calls"] filteredArrayUsingPredicate:
                        [NSPredicate predicateWithFormat:
                            @"SELF == 'default'"]] count] == 1;
            } else {
                ok = ok && [last[@"exit"] intValue] != 0 &&
                    [live[@"plugins"][@"com.saitoha.libsixel.quicklook.preview"]
                        isEqual:prior_live[@"plugins"]
                            [@"com.saitoha.libsixel.quicklook.preview"]] &&
                    ![live[@"calls"] containsObject:@"-r"];
            }
        } else if ([kind isEqual:@"activation-default"]) {
            ok = [activation[@"six"] isEqual:before[@"six"]] &&
                ![activation[@"calls"] containsObject:@"default"];
        } else if ([kind isEqual:@"default-identity"]) {
            ok = [chosen[@"six"][@"handler"] isEqual:
                @"com.saitoha.libsixel.quicklook.bridge"] &&
                [chosen[@"six"] isEqual:chosen[@"sixel"]];
        } else if ([kind isEqual:@"restored-default"]) {
            ok = [live[@"six"] isEqual:before[@"six"]] &&
                [journal[@"defaultRestore"] isEqual:@"restored"];
        } else if ([kind isEqual:@"providers-removed"]) {
            ok = [live[@"plugins"] count] == 2 &&
                [live[@"plugins"][@"com.saitoha.libsixel.quicklook.preview"]
                    count] == 0 &&
                [live[@"plugins"][@"com.saitoha.libsixel.quicklook.thumbnail"]
                    count] == 0;
        } else if ([kind isEqual:@"journal-deactivated"]) {
            ok = [journal[@"state"] isEqual:@"deactivated"] &&
                [journal[@"mode"] isEqual:@"deactivated"] &&
                [journal[@"createdProviders"] count] == 0 &&
                [journal[@"events"] count] > 0;
        } else if ([kind isEqual:@"refused-before-mutation"]) {
            ok = live != nil && [live[@"calls"] count] == 0 &&
                [last[@"exit"] intValue] != 0;
        } else if ([kind hasPrefix:@"error-"]) {
            ok = [last[@"exit"] intValue] ==
                [[kind substringFromIndex:6] intValue] &&
                [journal[@"state"] isEqual:@"pending"];
        } else if ([kind isEqual:@"default-not-called"]) {
            ok = [last[@"exit"] intValue] != 0 &&
                ![live[@"calls"] containsObject:@"default"];
        } else if ([kind isEqual:@"later-choice"]) {
            ok = [journal[@"defaultRestore"]
                isEqual:@"later-choice-preserved"] &&
                [live[@"six"] isEqual:before[@"six"]] &&
                [[live[@"calls"] filteredArrayUsingPredicate:
                    [NSPredicate predicateWithFormat:@"SELF == 'default'"]]
                    count] == 1;
        } else if ([kind isEqual:@"provider-not-removed"]) {
            ok = [last[@"exit"] intValue] != 0 &&
                ![live[@"calls"] containsObject:@"-r"];
        }
        if (!ok) {
            fprintf(stderr, "Lifecycle observation failed: %s / %s\n",
                argv[2], argv[3]);
        }
        return ok ? 0 : 1;
    }
}
