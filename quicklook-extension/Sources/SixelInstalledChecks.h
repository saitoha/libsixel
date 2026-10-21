/* Read-only installed observations. Distribution data is the expected side;
 * the installed manifest never authorizes its own bytes. Each invocation owns
 * one observation, including live OS queries, validation and output. */
#ifndef SIXEL_PLUGIN_TOOL
#define SIXEL_PLUGIN_TOOL @"/usr/bin/pluginkit"
#endif

static void
print_observation(id value)
{
    NSData *data;
    data = [NSJSONSerialization dataWithJSONObject:value
        options:NSJSONWritingSortedKeys error:nil];
    require(data != nil, @"Cannot serialize observation");
    fwrite(data.bytes, 1, data.length, stdout);
    putchar('\n');
}

static NSDictionary *
installed_paths(void)
{
    return @{@"host":bundle_paths()[2], @"bridge":bundle_paths()[3],
        @"preview":bundle_paths()[0], @"thumbnail":bundle_paths()[1],
        @"registration-helper":[support_path stringByAppendingPathComponent:
            @"register_sixel_preview"],
        @"management-helper":[support_path stringByAppendingPathComponent:
            @"sixel-quicklook-package"]};
}

static NSString *
installed_identifier(NSString *name)
{
    return [@"com.saitoha.libsixel.quicklook."
        stringByAppendingString:[name isEqual:@"host"] ? @"host" : name];
}

/* Query current process credentials and its actual Security session. The
 * console owner is an additional constraint, never a user to impersonate. */
#ifndef SIXEL_USER_PROBE
static NSDictionary *
observed_session(void)
{
    SecuritySessionId session;
    SessionAttributeBits bits;
    uid_t console_uid;
    gid_t console_gid;
    CFStringRef console;
    session = 0;
    bits = 0;
    console_uid = (uid_t)-1;
    require(SessionGetInfo(callerSecuritySession, &session, &bits) == noErr,
            @"Cannot query Security session");
    console = SCDynamicStoreCopyConsoleUser(NULL, &console_uid, &console_gid);
    if (console != NULL) {
        CFRelease(console);
    }
    return @{@"uid":@(getuid()), @"euid":@(geteuid()),
        @"consoleUID":@(console_uid), @"session":@(session),
        @"gui":@((bits & sessionHasGraphicAccess) != 0),
        @"remote":@((bits & sessionIsRemote) != 0)};
}

static NSDictionary *
observed_application(NSURL *url, BOOL need_signature)
{
    NSDictionary *plist;
    id code_identity;
    NSString *path;
    path = url.path;
    require(path != nil && url.isFileURL, @"LS returned a non-file URL");
    plist = [NSDictionary dictionaryWithContentsOfFile:
        [path stringByAppendingPathComponent:@"Contents/Info.plist"]];
    /* Preserve incomplete candidates as evidence, not valid identities. */
    code_identity = [NSNull null];
    if (need_signature) {
        @try { code_identity = signature(path); }
        @catch (NSException *exception) { (void)exception; }
    }
    return @{@"signature":code_identity, @"path":path,
        @"canonicalPath":path.stringByResolvingSymlinksInPath,
        @"identifier":plist[@"CFBundleIdentifier"] ?: [NSNull null],
        @"version":plist[@"CFBundleVersion"] ?: [NSNull null]};
}

static NSArray *
observed_ls(NSString *identifier)
{
    CFErrorRef error;
    CFArrayRef result;
    NSArray *urls;
    NSMutableArray *records;
    NSURL *url;
    error = NULL;
    result = LSCopyApplicationURLsForBundleIdentifier(
        (__bridge CFStringRef)identifier, &error);
    if (result == NULL) {
        require(error != NULL && CFErrorGetCode(error) ==
            kLSApplicationNotFoundErr, @"Launch Services query failed");
        CFRelease(error);
        return @[];
    }
    if (error != NULL) {
        CFRelease(error);
    }
    urls = CFBridgingRelease(result);
    records = [NSMutableArray array];
    for (url in urls) {
        [records addObject:observed_application(url, NO)];
    }
    return records;
}

static NSDictionary *
observed_default(NSString *extension)
{
    NSString *uti;
    NSString *handler;
    CFURLRef url;
    CFErrorRef error;
    NSDictionary *application;
    uti = CFBridgingRelease(UTTypeCreatePreferredIdentifierForTag(
        kUTTagClassFilenameExtension, (__bridge CFStringRef)extension, NULL));
    require(uti != nil, @"Cannot resolve extension UTI");
    handler = CFBridgingRelease(LSCopyDefaultRoleHandlerForContentType(
        (__bridge CFStringRef)uti, kLSRolesViewer));
    error = NULL;
    url = LSCopyDefaultApplicationURLForContentType((__bridge CFStringRef)uti,
        kLSRolesViewer, &error);
    if (url == NULL) {
        require(error != NULL && CFErrorGetCode(error) ==
            kLSApplicationNotFoundErr, @"Cannot query default application");
        application = @{};
    } else {
        application = observed_application(CFBridgingRelease(url), YES);
    }
    if (error != NULL) {
        CFRelease(error);
    }
    return @{@"uti":uti, @"handler":handler ?: [NSNull null],
        @"application":application};
}
#else
/* Private drivers supply read-only API responses at compile time. No shipped
 * command or environment variable can redirect OS/user queries to fixtures. */
#include SIXEL_USER_PROBE
#endif

static NSArray *
observed_plugins(NSString *identifier)
{
    NSString *text;
    NSString *line;
    NSString *trimmed;
    NSString *election;
    NSArray *fields;
    NSArray *matches;
    NSTextCheckingResult *match;
    NSRegularExpression *pattern;
    NSMutableArray *records;
    int status;
    text = run(SIXEL_PLUGIN_TOOL, @[@"-m", @"-A", @"-D", @"-v", @"-i",
        identifier], &status);
    require(status == 0, @"PlugInKit query failed");
    records = [NSMutableArray array];
    pattern = [NSRegularExpression regularExpressionWithPattern:
        @"^([+!=?\\-]?) *([^()]+)\\(([^()]+)\\)$" options:0 error:nil];
    for (line in [text componentsSeparatedByString:@"\n"]) {
        trimmed = [line stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceCharacterSet]];
        if (trimmed.length == 0) {
            continue;
        }
        if ([trimmed isEqual:@"(no matches)"]) {
            require([[text stringByTrimmingCharactersInSet:
                [NSCharacterSet whitespaceAndNewlineCharacterSet]]
                isEqual:@"(no matches)"], @"Mixed PlugInKit absence response");
            return @[];
        }
        /* macOS verbose match output is four tab-delimited columns. Reject
         * unknown output rather than accepting an ID substring or stale path.
         * Spaces inside an app path are not delimiters. Keep UUID/election. */
        fields = [trimmed componentsSeparatedByString:@"\t"];
        require(fields.count == 4, @"Unsupported PlugInKit match output");
        matches = [pattern matchesInString:fields[0] options:0
            range:NSMakeRange(0, [fields[0] length])];
        require(matches.count == 1, @"Invalid PlugInKit identity record");
        match = matches[0];
        require([[fields[0] substringWithRange:[match rangeAtIndex:2]]
            isEqual:identifier] && [fields[3] isAbsolutePath] &&
            [[NSUUID alloc] initWithUUIDString:fields[1]] != nil,
            @"Unexpected PlugInKit identity/path");
        election = [fields[0] substringWithRange:[match rangeAtIndex:1]];
        [records addObject:@{@"identifier":identifier,
            @"version":[fields[0] substringWithRange:[match rangeAtIndex:3]],
            @"uuid":fields[1], @"path":fields[3],
            @"canonicalPath":[fields[3] stringByResolvingSymlinksInPath],
            @"election":election.length ? election : @"default"}];
    }
    return records;
}

static NSDictionary *
user_state(void)
{
    return @{@"format":@1, @"session":observed_session(),
        @"host-ls":observed_ls(installed_identifier(@"host")),
        @"bridge-ls":observed_ls(installed_identifier(@"bridge")),
        @"preview-plugin":observed_plugins(installed_identifier(@"preview")),
        @"thumbnail-plugin":observed_plugins(
            installed_identifier(@"thumbnail")),
        @"six-default":observed_default(@"six"),
        @"sixel-default":observed_default(@"sixel")};
}

static NSDictionary *
expected_installed_manifest(NSString *directory)
{
    NSString *path;
    NSDictionary *manifest;
    NSDictionary *evidence;
    path = [directory stringByAppendingPathComponent:@"payload-manifest.json"];
    require(![path.stringByResolvingSymlinksInPath isEqual:
        manifest_path(install_root()).stringByResolvingSymlinksInPath],
        @"Expected manifest must come from the distribution");
    evidence = read_json([directory stringByAppendingPathComponent:
        @"package-evidence.json"]);
    require([hash_file(path) isEqual:evidence[@"manifestSHA256"]],
            @"Expected manifest differs from distribution evidence");
    manifest = read_json(path);
    validate_manifest(manifest);
    return manifest;
}

static NSDictionary *
expected_code(NSDictionary *manifest, NSString *path)
{
    NSDictionary *entry;
    for (entry in manifest[@"codes"]) {
        if ([entry[@"path"] isEqual:path]) {
            return entry;
        }
    }
    require(NO, @"Missing expected code identity");
    return nil;
}

static void
check_installed_tree(NSDictionary *manifest, NSString *relative)
{
    NSMutableArray *expected;
    NSDictionary *entry;
    NSString *prefix;
    NSString *path;
    struct stat attributes;
    expected = [NSMutableArray array];
    prefix = [relative stringByAppendingString:@"/"];
    for (entry in manifest[@"files"]) {
        path = entry[@"path"];
        if ([path isEqual:relative] || [path hasPrefix:prefix]) {
            [expected addObject:entry];
        }
    }
    require(expected.count != 0, @"Expected tree inventory is empty");
    require([payload_entries_for_roots(install_root(), @[relative])
        isEqual:expected], @"Installed tree differs from distribution");
    if ([install_root() isEqual:@"/"]) {
        for (entry in expected) {
            path = [install_root() stringByAppendingPathComponent:
                entry[@"path"]];
            require(lstat(path.fileSystemRepresentation, &attributes) == 0 &&
                attributes.st_uid == 0 && attributes.st_gid == 0,
                @"Installed entry must be root:wheel");
        }
    }
}

/* Context is an explicit, read-only input from a participating user workflow.
 * It is not created here and is not inferred from HOME, sudo or console user.
 * A session changes on logout; historical evidence is not a live session. */
static NSDictionary *
checked_user_context(NSString *file, NSString *directory)
{
    NSDictionary *context;
    NSDictionary *session;
    context = read_json(file);
    session = observed_session();
    require([context[@"format"] isEqual:@1] &&
        [context[@"mode"] isEqual:@"activated"] &&
        [context[@"manifestSHA256"] isEqual:hash_file([directory
            stringByAppendingPathComponent:@"payload-manifest.json"])],
        @"Invalid user context or distribution identity");
    require([context[@"uid"] isKindOfClass:[NSNumber class]] &&
        [context[@"session"] isKindOfClass:[NSNumber class]] &&
        [context[@"uid"] unsignedIntValue] != 0 &&
        [context[@"uid"] isEqual:session[@"uid"]] &&
        [context[@"uid"] isEqual:session[@"euid"]] &&
        [context[@"uid"] isEqual:session[@"consoleUID"]] &&
        [context[@"session"] isEqual:session[@"session"]] &&
        [session[@"gui"] isEqual:@YES] &&
        [session[@"remote"] isEqual:@NO],
        @"Target UID/GUI session mismatch or unavailable");
    return context;
}

static void
check_registered_path(NSArray *records, NSString *name,
                      NSDictionary *manifest)
{
    NSString *relative;
    NSString *expected;
    NSDictionary *record;
    NSDictionary *identity;
    NSDictionary *plist;
    BOOL found;
    relative = installed_paths()[name];
    expected = [install_root() stringByAppendingPathComponent:relative];
    found = NO;
    for (record in records) {
        if ([record[@"path"] isEqual:expected] &&
            [record[@"canonicalPath"] isEqual:expected] &&
            [record[@"identifier"] isEqual:installed_identifier(name)] &&
            [record[@"version"] isEqual:manifest[@"packageVersion"]]) {
            found = YES;
        }
    }
    require(found, @"Expected installed instance is not registered");
    check_ancestors(install_root(), relative);
    plist = [NSDictionary dictionaryWithContentsOfFile:[expected
        stringByAppendingPathComponent:@"Contents/Info.plist"]];
    require([plist[@"CFBundleIdentifier"] isEqual:installed_identifier(name)]
        && [plist[@"CFBundleVersion"] isEqual:manifest[@"packageVersion"]],
        @"Registered file identity/version differs from distribution");
    identity = expected_code(manifest, relative);
    require([signature(expected) isEqual:identity[@"signature"]],
        @"Registered code identity differs from distribution");
}

/* The observer can compare a journal but cannot establish its provenance.
 * Require complete typed identities, or explicit known absence, so a string
 * or an empty object cannot silently become restoration evidence. */
static void
check_journal_value(id value, BOOL election)
{
    NSDictionary *app;
    NSString *key;
    require([value isKindOfClass:[NSDictionary class]],
            @"Journal value must be an identity record");
    if (election) {
        if ([value[@"absent"] isEqual:@YES] && [value count] == 1) {
            return;
        }
        for (key in @[@"identifier", @"version", @"uuid", @"path",
                       @"canonicalPath", @"election"]) {
            require([value[key] isKindOfClass:[NSString class]] &&
                [value[key] length] != 0, @"Incomplete election identity");
        }
        require([[NSUUID alloc] initWithUUIDString:value[@"uuid"]] != nil &&
            [value[@"path"] isAbsolutePath] &&
            [value[@"canonicalPath"] isAbsolutePath] &&
            [@[@"+", @"-", @"!", @"=", @"?", @"default"]
                containsObject:value[@"election"]],
            @"Invalid election identity");
        return;
    }
    require([value[@"uti"] isKindOfClass:[NSString class]] &&
        [value[@"uti"] length] != 0 &&
        [value[@"application"] isKindOfClass:[NSDictionary class]],
        @"Incomplete default identity");
    app = value[@"application"];
    if ([value[@"handler"] isEqual:[NSNull null]] && app.count == 0) {
        return;
    }
    require([value[@"handler"] isKindOfClass:[NSString class]] &&
        [value[@"handler"] length] != 0 &&
        [app[@"signature"] isKindOfClass:[NSDictionary class]] &&
        [app[@"signature"][@"cdhash"] isKindOfClass:[NSString class]] &&
        [app[@"signature"][@"cdhash"] length] != 0,
        @"Default application signature identity is insufficient");
    for (key in @[@"identifier", @"version", @"path", @"canonicalPath"]) {
        require([app[key] isKindOfClass:[NSString class]] &&
            [app[key] length] != 0, @"Incomplete default application identity");
    }
    require([app[@"path"] isAbsolutePath] &&
        [app[@"canonicalPath"] isAbsolutePath] &&
        [value[@"handler"] isEqual:app[@"identifier"]],
        @"Default handler and application identity disagree");
}

static int
observe_installed(NSString *observation, NSString *directory,
                  NSString *context_file)
{
    NSDictionary *manifest;
    NSDictionary *receipt;
    NSDictionary *context;
    NSDictionary *current;
    NSDictionary *record;
    NSArray *records;
    NSArray *parts;
    NSString *name;
    NSString *kind;
    NSString *relative;
    NSString *path;
    struct stat attributes;
    BOOL user;
    require_no_lock(managed_lock_path());
    require_no_lock(lock_path());
    manifest = expected_installed_manifest(directory);
    parts = [observation componentsSeparatedByString:@":"];
    require(parts.count == 2, @"Invalid installed observation");
    kind = parts[0];
    name = parts[1];
    user = [@[@"session", @"ls", @"plugin", @"duplicates", @"election",
               @"default"] containsObject:kind];
    if (user && context_file.length == 0) {
        puts("User observation unavailable: system-only; no explicit context");
        return 77;
    }
    context = user ? checked_user_context(context_file, directory) : nil;
    if ([kind isEqual:@"receipt"]) {
        receipt = read_receipt();
        require(receipt.count != 0, @"Exact package receipt is absent");
        if ([name isEqual:@"identity"]) {
            require([receipt[@"pkgid"] isEqual:package_id],
                    @"Receipt identifier mismatch");
        } else if ([name isEqual:@"version"]) {
            require([receipt[@"pkg-version"]
                isEqual:manifest[@"packageVersion"]],
                @"Receipt version mismatch");
        } else {
            require([name isEqual:@"location"] &&
                [receipt[@"volume"] isEqual:@"/"] &&
                [@[@"/", @""] containsObject:receipt[@"install-location"]],
                @"Receipt location mismatch");
        }
        print_observation(receipt);
    } else if ([kind isEqual:@"manifest"]) {
        require([name isEqual:@"bytes"], @"Unknown manifest observation");
        path = manifest_path(install_root());
        check_ancestors(install_root(), [support_path
            stringByAppendingPathComponent:@"payload-manifest.json"]);
        require([read_data(path) isEqual:read_data([directory
            stringByAppendingPathComponent:@"payload-manifest.json"])] &&
            lstat(path.fileSystemRepresentation, &attributes) == 0 &&
            S_ISREG(attributes.st_mode) &&
            (attributes.st_mode & 07777) == 0644 &&
            (![install_root() isEqual:@"/"] ||
                (attributes.st_uid == 0 && attributes.st_gid == 0)),
            @"Installed manifest bytes/attributes mismatch");
    } else if ([kind isEqual:@"path"] || [kind isEqual:@"payload"] ||
               [kind isEqual:@"signature"]) {
        relative = installed_paths()[name];
        if ([name isEqual:@"support"] && [kind isEqual:@"payload"]) {
            relative = support_path;
        }
        require(relative != nil, @"Unknown installed component");
        path = [install_root() stringByAppendingPathComponent:relative];
        check_ancestors(install_root(), relative);
        if ([kind isEqual:@"path"]) {
            require([bundle_paths() containsObject:relative] &&
                lstat(path.fileSystemRepresentation, &attributes) == 0 &&
                S_ISDIR(attributes.st_mode), @"Installed bundle path missing");
        } else if ([kind isEqual:@"payload"]) {
            check_installed_tree(manifest, relative);
        } else {
            require([signature(path) isEqual:
                expected_code(manifest, relative)[@"signature"]],
                @"Installed signature identity mismatch");
        }
    } else if ([kind isEqual:@"session"]) {
        require([name isEqual:@"identity"], @"Unknown session observation");
        print_observation(observed_session());
    } else if ([kind isEqual:@"ls"] || [kind isEqual:@"plugin"] ||
               [kind isEqual:@"duplicates"] || [kind isEqual:@"election"]) {
        require([@[@"host", @"bridge", @"preview", @"thumbnail"]
            containsObject:name], @"Unknown registered component");
        records = [@[@"host", @"bridge"] containsObject:name] ?
            observed_ls(installed_identifier(name)) :
            observed_plugins(installed_identifier(name));
        if ([kind isEqual:@"duplicates"]) {
            require(records.count == 1, @"Missing or duplicate registration");
        } else {
            check_registered_path(records, name, manifest);
        }
        if ([kind isEqual:@"election"]) {
            require(records.count == 1, @"Election instance is ambiguous");
            record = records[0];
            check_journal_value(context[@"before"][observation], YES);
            check_journal_value(context[@"owned"][observation], YES);
            require([context[@"before"] isKindOfClass:[NSDictionary class]] &&
                [context[@"owned"] isKindOfClass:[NSDictionary class]] &&
                context[@"before"][observation] != nil &&
                [context[@"owned"][observation] isEqual:record],
                @"Election journal missing or current differs from owned");
        }
        print_observation(@{@"current":records,
            @"before":context[@"before"][observation] ?: [NSNull null],
            @"owned":context[@"owned"][observation] ?: [NSNull null]});
    } else if ([kind isEqual:@"default"]) {
        require([@[@"six", @"sixel"] containsObject:name],
            @"Unknown default extension");
        current = observed_default(name);
        check_journal_value(context[@"before"][observation], NO);
        check_journal_value(context[@"owned"][observation], NO);
        require([context[@"before"] isKindOfClass:[NSDictionary class]] &&
            [context[@"owned"] isKindOfClass:[NSDictionary class]] &&
            context[@"before"][observation] != nil &&
            [context[@"owned"][observation] isEqual:current],
            @"Default journal missing or current differs from owned");
        print_observation(@{@"current":current,
            @"before":context[@"before"][observation],
            @"owned":context[@"owned"][observation]});
    } else {
        require(NO, @"Unknown installed observation");
    }
    return 0;
}
