/* Private state endpoints injected inside the lifecycle driver. Keep the
 * .inc.h suffix: these bodies require earlier driver declarations and must
 * not be hoisted into the common amalgamation header preamble. The driver
 * uses real payload/Security and journal code. */
static NSDictionary *
private_state(void)
{
    return read_json([test_root stringByAppendingPathComponent:@"live.json"]);
}

static NSDictionary *
observed_session(void)
{
    return private_state()[@"session"];
}

static NSArray *
observed_ls(NSString *identifier)
{
    return private_state()[@"ls"][identifier] ?: @[];
}

static NSDictionary *
observed_default(NSString *extension)
{
    return private_state()[extension];
}
