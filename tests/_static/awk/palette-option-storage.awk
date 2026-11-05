# Quantizer option getters access shared defaults only through the cache helper.
function strip(line,    result, token) {
    while (line != "") {
        if (comment) {
            if (!match(line, /\*\//)) break
            line = substr(line, RSTART + RLENGTH)
            comment = 0
        } else if (match(line, /"([^"\\]|\\.)*"|'([^'\\]|\\.)*'|\/\*|\/\//)) {
            result = result substr(line, 1, RSTART - 1)
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (token == "//") break
            if (token == "/*") comment = 1
            result = result " "
        } else {
            result = result line
            break
        }
    }
    return result
}
function inspect(code,    lines, count, i) {
    pending = depth = active = getters = 0
    count = split(code, lines, "\n")
    for (i = 1; i <= count; ++i)
        if (consume(lines[i])) return 1
    return 0
}
function consume(code,    tokens, n, i, token) {
    if (active && code ~ /^[[:space:]]*static (int cache_loaded|(unsigned int|[[:alnum:]_]+) cache_value);[[:space:]]*$/)
        return 0
    # Only these two address operands may expose persistent cache storage.
    # Other reads, writes and aliases bypass the helper's environment lock.
    gsub(/sixel_palette_option_cache\(&cache_loaded, &cache_value,/, "sixel_palette_option_cache(", code)
    # Ignore expressions inside other functions, and ordinary statements
    # inside getters. Only scope delimiters and static storage matter.
    if (depth && !active && code !~ /[{}]/) return 0
    if (depth && active && code !~ /[{}]|(^|[^[:alnum:]_])(static|cache_loaded|cache_value)([^[:alnum:]_]|$)/) return 0
    gsub(/[^[:alnum:]_[:space:]]/, " & ", code)
    n = split(code, tokens, /[[:space:]]+/)
    for (i = 1; i <= n; ++i) {
        token = tokens[i]
        if (depth) {
            if (active && (token == "static" || token == "cache_loaded" ||
                           token == "cache_value")) return 1
            if (token == "{") ++depth
            if (token == "}") --depth
        } else if (token ~ /^sixel_(get_k(means|medoids|center)_|k(means|medoids|center)_get_)/) {
            pending = 1
        } else if (token == "{") {
            depth = 1
            active = pending
            pending = 0
            if (active) ++getters
        } else if (token == ";") {
            pending = 0
        }
    }
    return 0
}
BEGIN {
    prefix = "sixel_get_kmeans_init_type(void) { "
    if (!inspect(prefix "static int ready; return ready; }") ||
        !inspect(prefix "if (1) { static\nint value; } }") ||
        !inspect("sixel_kmedoids_get_auction_from_env(void) { static int x; }") ||
        !inspect(prefix "cache_loaded = 1; }") ||
        !inspect(prefix "return cache_value; }") ||
        !inspect(prefix "other(&cache_value); }") ||
        inspect(prefix "\nstatic int cache_loaded;\nstatic int cache_value;\n" \
            "sixel_palette_option_cache(&cache_loaded, &cache_value, &v, sizeof(v), 1);\n}") ||
        inspect(prefix "int value = 1; return value; }") ||
        inspect("static int other(void) { static int value; }") ||
        inspect("int other(void) { sixel_get_kmeans_iter(); { static int x; } }") ||
        inspect("int sixel_get_kcenter_algo(void); static int other;")) {
        print "palette option storage controls failed"
        failed = 1
        exit 1
    }
    sample = strip("/* static */ const char *s = \"static\"; // static")
    if (inspect(prefix sample " }")) {
        print "palette option comment/literal controls failed"
        failed = 1
        exit 1
    }
}
FNR == 1 { comment = pending = depth = active = getters = 0 }
{
    if (consume(strip($0))) violations[FILENAME] = 1
    file_getters[FILENAME] = getters
}
END {
    for (file in file_getters) {
        if (violations[file] || !file_getters[file]) {
            print file ": quantizer defaults require synchronized cache access"
            failed = 1
        }
    }
    exit failed
}
