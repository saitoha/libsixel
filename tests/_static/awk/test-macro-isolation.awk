# Function-like test macros share one namespace in the amalgamated runner.
# Inspect every conditional branch; do not attempt to evaluate target macros.
function reset(    key) {
    for (key in variants) delete variants[key]
    for (key in variant_count) delete variant_count[key]
    for (key in units) delete units[key]
    for (key in unit_count) delete unit_count[key]
    for (key in leaked) delete leaked[key]
    for (key in active) delete active[key]
}
function begin_unit(path,    key) {
    file = path
    comment = 0
    pending = ""
    for (key in active) delete active[key]
}
function end_unit(    name) {
    for (name in active) leaked[name] = file
}
function strip_comments(line,    result, token) {
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
            if (token == "/*") {
                comment = 1
                result = result " "
            } else result = result token
        } else {
            result = result line
            break
        }
    }
    return result
}
function consume(line,    name, body, key) {
    line = pending line
    if (sub(/\\$/, "", line)) {
        pending = line
        return
    }
    pending = ""
    # Still inspect comment openers on ordinary lines so directives inside
    # multiline comments cannot masquerade as definitions.
    if (!comment && line !~ /^[[:space:]]*#/ && line !~ /\/\*/) return
    line = strip_comments(line)
    sub(/^[[:space:]]*/, "", line)
    if (line ~ /^#[[:space:]]*undef[[:space:]]+/) {
        sub(/^#[[:space:]]*undef[[:space:]]+/, "", line)
        sub(/[^A-Za-z0-9_].*$/, "", line)
        delete active[line]
        return
    }
    if (line !~ /^#[[:space:]]*define[[:space:]]+/) return
    sub(/^#[[:space:]]*define[[:space:]]+/, "", line)
    name = line
    sub(/[^A-Za-z0-9_].*$/, "", name)
    body = substr(line, length(name) + 1)
    if (substr(body, 1, 1) != "(") return
    sub(/[[:space:]]*$/, "", body)
    active[name] = 1
    key = name SUBSEP file
    if (!(key in units)) {
        units[key] = 1
        ++unit_count[name]
    }
    key = name SUBSEP body
    if (!(key in variants)) {
        variants[key] = 1
        ++variant_count[name]
    }
}
function violations(report,    name, errors) {
    for (name in leaked) {
        if (unit_count[name] > 1 && variant_count[name] > 1) {
            if (report)
                print leaked[name] ": macro " name " escapes its source unit and has differing definitions; use a private name and #undef"
            ++errors
        }
    }
    return errors
}
function specimen(path, text,    lines, count, i) {
    begin_unit(path)
    count = split(text, lines, "\n")
    for (i = 1; i <= count; ++i) consume(lines[i])
    end_unit()
}
BEGIN {
    specimen("a", "#define CHECK(x) first(x)")
    specimen("b", "#define CHECK(x) second(x)")
    if (!violations(0)) control_failed = 1
    reset()
    specimen("a", "#define CHECK(x) first(x)")
    specimen("b", "#define CHECK(x) first(x)")
    if (violations(0)) control_failed = 1
    reset()
    specimen("a", "#define CHECK(x) first(x)\n#undef CHECK")
    specimen("b", "#define CHECK(x) second(x)\n#undef CHECK")
    if (violations(0)) control_failed = 1
    reset()
    specimen("a", "#define CHECK(x) first(x)")
    specimen("b", "#define CHECK(x) second(x)\n#undef CHECK")
    if (!violations(0)) control_failed = 1
    reset()
    specimen("a", "/*\n#define CHECK(x) first(x)\n*/")
    specimen("b", "#define CHECK(x) second(x)")
    if (violations(0)) control_failed = 1
    reset()
    specimen("a", "#define CHECK(x) \\\nfirst(x)")
    specimen("b", "#define CHECK(x) \\\nsecond(x)")
    if (!violations(0)) control_failed = 1
    reset()
    if (control_failed) {
        print "test macro isolation scanner controls failed"
        exit 1
    }
}
# The input is a path inventory. Reading units here retains cross-file state
# without putting thousands of source paths on the process command line.
{
    begin_unit($0)
    while ((status = getline source_line < file) > 0) consume(source_line)
    if (status < 0) {
        print file ": cannot read test source"
        failed = 1
    }
    close(file)
    end_unit()
}
END { if (failed || violations(1)) exit 1 }
