/* Independent private OS state, with failure injection and a write-ahead
 * witness. Product code must publish pending intent before any mutation.
 * This .inc.h body belongs inside the driver, not the amalgamation preamble. */
static NSMutableDictionary *
private_before_mutation(NSString *operation)
{
    NSMutableDictionary *state;
    NSMutableArray *calls;
    NSDictionary *journal;
    state = [private_state() mutableCopy];
    journal = read_json([test_journal
        stringByAppendingPathComponent:@"journal.json"]);
    require([journal[@"state"] isEqual:@"pending"] ||
        [journal[@"state"] isEqual:@"restore-pending"],
        @"Private witness: mutation without durable pending journal");
    calls = [state[@"calls"] mutableCopy] ?: [NSMutableArray array];
    [calls addObject:operation];
    state[@"calls"] = calls;
    write_json(state, [test_root stringByAppendingPathComponent:@"live.json"]);
    return state;
}

static int
user_register(NSString *path)
{
    NSMutableDictionary *state;
    NSMutableDictionary *ls;
    NSDictionary *app;
    state = private_before_mutation(@"register");
    if ([state[@"fail"] isEqual:@"register"]) {
        return 41;
    }
    app = state[@"applications"][path];
    require(app != nil, @"Private registration received foreign path");
    ls = [state[@"ls"] mutableCopy];
    ls[app[@"identifier"]] = @[app];
    state[@"ls"] = ls;
    /* Optional explicit OS response for migration controls. Registration is
     * not assumed to select this app; negative fixtures omit this response. */
    if (state[@"registrationDefaults"][path] != nil) {
        state[@"six"] = state[@"registrationDefaults"][path];
        state[@"sixel"] = state[@"registrationDefaults"][path];
    }
    write_json(state, [test_root stringByAppendingPathComponent:@"live.json"]);
    return 0;
}

static int
user_default(NSString *path, NSString *uti)
{
    NSMutableDictionary *state;
    NSDictionary *app;
    NSDictionary *value;
    state = private_before_mutation(@"default");
    if ([state[@"fail"] isEqual:@"default-wide-status"]) {
        return 256;
    }
    if ([state[@"fail"] isEqual:@"default"]) {
        return 42;
    }
    if ([state[@"fail"] isEqual:@"default-readback"]) {
        return 0;
    }
    app = state[@"applications"][path];
    require(app != nil, @"Private default received foreign path");
    value = @{@"uti":uti, @"handler":app[@"identifier"], @"application":app};
    state[@"six"] = value;
    state[@"sixel"] = value;
    write_json(state, [test_root stringByAppendingPathComponent:@"live.json"]);
    return 0;
}
