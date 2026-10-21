/* A lifecycle is an explicit operator-supplied sequence, not a dependency of
 * installcheck. Privileged stages and GUI-user stages use separate explicitly
 * selected executors. We never synthesize sudo, launchctl or a target user. */
static NSArray *
lifecycle_command(NSDictionary *plan, NSString *name)
{
    NSArray *command;
    id argument;
    command = plan[name];
    require([command isKindOfClass:[NSArray class]] && command.count > 0,
        [@"Missing explicit lifecycle command: " stringByAppendingString:name]);
    for (argument in command) {
        require([argument isKindOfClass:[NSString class]] &&
            [argument length] > 0, @"Invalid lifecycle argument");
    }
    require([command[0] isAbsolutePath],
        @"Lifecycle executors must be absolute explicit paths");
    return command;
}

static void
lifecycle_stage(int fd, NSMutableDictionary *record,
                NSDictionary *plan, NSString *name, int *status,
                BOOL *attempted)
{
    NSArray *command;
    NSMutableDictionary *event;
    NSString *output;
    double start;
    command = lifecycle_command(plan, name);
    event = [@{@"stage":name, @"command":command,
        @"state":@"pending"} mutableCopy];
    [record[@"stages"] addObject:event];
    user_journal_write(fd, record);
    start = monotonic_seconds();
    /* Publish the observed status directly to the caller. Completion logging
     * can fail after the child exits; returning a local value would lose it.
     * No command is attempted before its intent is durably recorded. */
    *status = 1;
    if (attempted != NULL) {
        *attempted = YES;
    }
    @try {
        output = run(command[0], [command subarrayWithRange:
            NSMakeRange(1, command.count - 1)], status);
        fputs(output.UTF8String, stderr);
    } @catch (NSException *exception) {
        event[@"error"] = exception.reason;
    }
    event[@"seconds"] = @(monotonic_seconds() - start);
    event[@"status"] = @(*status);
    event[@"state"] = @"completed";
    user_journal_write(fd, record);
}

static int
run_lifecycle(NSString *plan_file, NSString *record_directory)
{
    NSDictionary *plan;
    NSMutableDictionary *record;
    NSString *stage;
    int fd;
    int status;
    int cleanup;
    int recording_status;
    BOOL installed;
    BOOL activation_attempted;
    BOOL cleanup_attempted;
    BOOL cleanup_finished;
    plan = read_json(plan_file);
    require([plan[@"format"] isEqual:@1] &&
        [plan[@"explicitLifecycle"] isEqual:@YES],
        @"Explicit version1 lifecycle plan required");
    require(getenv("DESTDIR") == NULL || strlen(getenv("DESTDIR")) == 0,
        @"Lifecycle is not available with DESTDIR");
    require(getenv("SIXEL_INSTALLCHECK_DESTDIR") == NULL ||
        strlen(getenv("SIXEL_INSTALLCHECK_DESTDIR")) == 0,
        @"Lifecycle cannot run as a staging observation");
    for (stage in @[@"check", @"install", @"activate", @"observe",
        @"deactivate", @"remove", @"restore", @"verify-restored"]) {
        lifecycle_command(plan, stage);
    }
    if (plan[@"set-default"] != nil) {
        lifecycle_command(plan, @"set-default");
    }
    fd = user_journal_open(record_directory);
    status = 0;
    cleanup = 0;
    recording_status = 0;
    cleanup_attempted = NO;
    cleanup_finished = NO;
    installed = NO;
    activation_attempted = NO;
    record = [@{@"format":@1, @"kind":@"explicit-lifecycle",
        @"planSHA256":hash_file(plan_file), @"stages":[NSMutableArray array],
        @"state":@"running"} mutableCopy];
    @try {
        require(user_journal_read(fd) == nil,
            @"Lifecycle evidence directory already contains a run");
        user_journal_write(fd, record);
        for (stage in @[@"check", @"install", @"activate", @"observe"]) {
            if ([stage isEqual:@"activate"]) {
                activation_attempted = YES;
            }
            lifecycle_stage(fd, record, plan, stage, &status, NULL);
            if (status != 0) {
                break;
            }
            if ([stage isEqual:@"install"]) {
                installed = YES;
            }
            if ([stage isEqual:@"activate"] && plan[@"set-default"] != nil) {
                lifecycle_stage(fd, record, plan, @"set-default",
                    &status, NULL);
                if (status != 0) {
                    break;
                }
            }
        }
        /* Unknown Installer outcome is not permission to remove files. A
         * failed user cleanup likewise stops system removal/restoration. */
        if (activation_attempted) {
            lifecycle_stage(fd, record, plan, @"deactivate",
                &cleanup, &cleanup_attempted);
        }
        if (installed && cleanup == 0) {
            lifecycle_stage(fd, record, plan, @"remove",
                &cleanup, &cleanup_attempted);
            if (cleanup == 0) {
                lifecycle_stage(fd, record, plan, @"restore",
                    &cleanup, &cleanup_attempted);
            }
            if (cleanup == 0) {
                lifecycle_stage(fd, record, plan, @"verify-restored",
                    &cleanup, &cleanup_attempted);
            }
        }
        cleanup_finished = YES;
        record[@"originalStatus"] = @(status);
        record[@"cleanupStatus"] = cleanup_attempted ?
            @(cleanup) : [NSNull null];
        record[@"cleanupState"] = !cleanup_attempted ? @"not-attempted" :
            cleanup == 0 ? @"completed" : @"failed";
        record[@"state"] = status == 0 && cleanup == 0 ?
            @"completed" : @"incomplete";
        user_journal_write(fd, record);
        print_observation(record);
    } @catch (NSException *exception) {
        fprintf(stderr, "Lifecycle incomplete: %s\n",
            exception.reason.UTF8String);
        /* Persistence failure must stop further mutation, but must not erase
         * a known command failure. stderr is separate, non-durable evidence;
         * the last published journal can still contain a pending intent. */
        fprintf(stderr, "Lifecycle outcomes: originalStatus=%d ", status);
        if (cleanup_attempted) {
            fprintf(stderr, "cleanupStatus=%d cleanupState=%s ", cleanup,
                !cleanup_finished ? "interrupted" :
                    cleanup == 0 ? "completed" : "failed");
        } else {
            fputs("cleanupStatus=null cleanupState=not-attempted ", stderr);
        }
        fputs("recordingFailure=yes\n", stderr);
        recording_status = 1;
    } @finally {
        close(fd);
    }
    return status != 0 ? status :
        cleanup != 0 ? cleanup : recording_status;
}
