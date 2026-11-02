# A strict JSON reader for exact approved pattern exceptions. Do not evaluate
# JSON as shell code or accept unknown fields, wildcards, or stale identities.
function json_space() { while (substr(json, pos, 1) ~ /^[ \t\r\n]$/) pos++ }
function json_string(    result, ch, escape, digits, value, i) {
    if (substr(json, pos++, 1) != "\"") { invalid = 1; return "" }
    while (pos <= length(json)) {
        ch = substr(json, pos++, 1)
        if (ch == "\"") return result
        if (ch ~ /[[:cntrl:]]/) { invalid = 1; return "" }
        if (ch == "\\") {
            escape = substr(json, pos++, 1)
            if (escape == "\"" || escape == "\\" || escape == "/") ch = escape
            else if (escape == "b") ch = "\b"
            else if (escape == "f") ch = "\f"
            else if (escape == "n") ch = "\n"
            else if (escape == "r") ch = "\r"
            else if (escape == "t") ch = "\t"
            else if (escape == "u") {
                digits = substr(json, pos, 4); pos += 4
                if (length(digits) != 4 || digits ~ /[^0-9a-fA-F]/) { invalid = 1; return "" }
                value = 0
                for (i = 1; i <= 4; i++) value = value * 16 + index("0123456789abcdef", tolower(substr(digits, i, 1))) - 1
                if (value < 128) ch = sprintf("%c", value)
                else if (value < 2048) ch = sprintf("%c%c", 192 + int(value / 64), 128 + value % 64)
                else ch = sprintf("%c%c%c", 224 + int(value / 4096), 128 + int(value / 64) % 64, 128 + value % 64)
            } else { invalid = 1; return "" }
        }
        result = result ch
    }
    invalid = 1
    return ""
}
function json_value(path, depth,    ch, name, count, token) {
    if (depth > 8) { invalid = 1; return }
    json_space(); ch = substr(json, pos, 1)
    if (ch == "{" || ch == "[") {
        kind[path] = ch; pos++; json_space()
        if (substr(json, pos, 1) == (ch == "{" ? "}" : "]")) { pos++; return }
        while (!invalid) {
            json_space()
            if (ch == "{") {
                name = json_string(); json_space()
                if (substr(json, pos++, 1) != ":") { invalid = 1; return }
            } else name = ++count
            if ((path SUBSEP name) in kind) { invalid = 1; return }
            json_value(path SUBSEP name, depth + 1)
            members[path]++; json_space()
            token = substr(json, pos++, 1)
            if (token == (ch == "{" ? "}" : "]")) return
            if (token != ",") { invalid = 1; return }
        }
    } else if (ch == "\"") { kind[path] = "string"; value[path] = json_string() }
    else {
        token = substr(json, pos)
        if (!match(token, /^-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?/)) { invalid = 1; return }
        token = substr(token, 1, RLENGTH); pos += RLENGTH
        kind[path] = "number"; value[path] = token
    }
}
function whitelist_document(text, quiet,    key, i, entry, path, line, digest, reason, approval) {
    for (key in kind) delete kind[key]
    for (key in value) delete value[key]
    for (key in members) delete members[key]
    for (key in approved) delete approved[key]
    invalid = 0; json = text; pos = 1
    json_value("root", 0); json_space()
    if (pos <= length(json) || kind["root"] != "{" || members["root"] != 2 ||
        kind["root" SUBSEP "version"] != "number" || value["root" SUBSEP "version"] != "1" ||
        kind["root" SUBSEP "entries"] != "[") invalid = 1
    for (i = 1; !invalid && i <= members["root" SUBSEP "entries"]; i++) {
        entry = "root" SUBSEP "entries" SUBSEP i
        path = entry SUBSEP "path"; line = entry SUBSEP "line"; digest = entry SUBSEP "sha256"
        reason = entry SUBSEP "reason"; approval = entry SUBSEP "approval"
        if (kind[entry] != "{" || members[entry] != 5 || kind[path] != "string" ||
            value[path] !~ /^tests\/.*\.t$/ || value[path] ~ /(^|\/)\.\.(\/|$)|[*?\[\\]/ ||
            kind[line] != "number" || value[line] !~ /^[1-9][0-9]*$/ ||
            kind[digest] != "string" || length(value[digest]) != 64 || value[digest] ~ /[^a-f0-9]/ ||
            kind[reason] != "string" || value[reason] !~ /[^[:space:]]/ ||
            kind[approval] != "string" || value[approval] !~ /[^[:space:]]/) { invalid = 1; break }
        key = value[path] SUBSEP value[line] SUBSEP value[digest]
        if ((key in approved) || !(key in finding)) { invalid = 1; break }
        approved[key] = 1
    }
    if (invalid && !quiet) print "# invalid, duplicate, or stale pattern whitelist entry"
    return !invalid
}
function validate_whitelist(path,    text, line, status) {
    while ((status = (getline line < path)) > 0) text = text line "\n"
    close(path)
    if (status < 0 || !whitelist_document(text, 0)) failed = 1
}
