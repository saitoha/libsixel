/* Private state endpoints. These headers are never included in the shipped
 * executable. The driver uses real payload/Security and journal code. */
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
