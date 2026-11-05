# SPDX-License-Identifier: MIT
# Reuse the declaration lexer in definition-only mode. Inspect literal direct
# includes without compiling or evaluating conditional preprocessor branches.
function read_definitions(path,    line, number, result) {
    if (path in scanned_source) return
    scanned_source[path] = 1
    start_header(path)
    number = 0
    while ((result = (getline line < path)) > 0) scan_line(line, ++number)
    close(path)
    if (result < 0) {
        print "# cannot read included source: " path
        failed = 1
    }
}
function isolated(name,    value) {
    value = alias[name]
    return value ~ /^[A-Za-z_][A-Za-z0-9_]*$/ && value != name
}
function directive_line(line, path, number,    parts, source, key, fields) {
    if (line ~ /^#[[:space:]]*define[[:space:]]/) {
        sub(/^#[[:space:]]*define[[:space:]]+/, "", line)
        split(line, parts, /[[:space:]]+/)
        alias[parts[1]] = parts[2]
    } else if (line ~ /^#[[:space:]]*undef[[:space:]]/) {
        sub(/^#[[:space:]]*undef[[:space:]]+/, "", line)
        delete alias[line]
    } else if (line ~ /^#[[:space:]]*include[[:space:]]+"src\/[^" ]+\.c"$/) {
        source = line
        sub(/^[^"]*"/, "", source)
        sub(/"$/, "", source)
        source = src_root "/" source
        read_definitions(source)
        for (key in external_definition) {
            split(key, fields, SUBSEP)
            if (fields[1] == source && !isolated(fields[2])) {
                print "# " path ":" number ": " fields[2] \
                    " needs a private alias before including " source
                failed = 1
            }
        }
    }
}
# Preserve string literals (including include paths), remove comments, and
# join directive continuations before interpreting aliases and include sites.
function clean_line(line,    i, ch, next_ch, output, quoted, escape) {
    output = ""
    for (i = 1; i <= length(line); ++i) {
        ch = substr(line, i, 1)
        next_ch = substr(line, i + 1, 1)
        if (comment) {
            if (ch == "*" && next_ch == "/") { comment = 0; ++i }
        } else if (quoted) {
            output = output ch
            if (escape) escape = 0
            else if (ch == "\\") escape = 1
            else if (ch == quoted) quoted = ""
        } else if (ch == "/" && next_ch == "*") {
            comment = 1
            output = output " "
            ++i
        } else if (ch == "/" && next_ch == "/") {
            break
        } else {
            output = output ch
            if (ch == "\"" || ch == sprintf("%c", 39)) quoted = ch
        }
    }
    return output
}
BEGIN {
    collect_definitions = 1
    start_header("control")
    scan_line("static int local(void) { return 0; }", 1)
    scan_line("int prototype(void);", 2)
    scan_line("int visible(int values[static 3]) { return values[0]; }", 3)
    scan_line("/* int false_function(void) { } */", 4)
    scan_line("__attribute__((unused)) int attributed(void) { return 0; }", 5)
    if (!("control" SUBSEP "visible" in external_definition) ||
        !("control" SUBSEP "attributed" in external_definition) ||
        ("control" SUBSEP "local" in external_definition) ||
        ("control" SUBSEP "prototype" in external_definition) ||
        ("control" SUBSEP "false_function" in external_definition)) {
        print "# source definition controls failed"
        failed = 1
    }
    directive_line("#define visible private_visible", "control", 1)
    if (!isolated("visible")) failed = 1
    directive_line("#undef visible", "control", 2)
    if (isolated("visible")) failed = 1
    directive_line("#define visible visible", "control", 3)
    if (isolated("visible")) failed = 1
}
{
    path = $0
    # Most test files never include production C. Avoid lexing their macros
    # and literals; the full pass below validates the candidate include sites.
    candidate = 0
    while ((result = (getline line < path)) > 0) {
        if (line ~ /^[[:space:]]*#[[:space:]]*include[[:space:]]+"src\/[^" ]+\.c"/) {
            candidate = 1
            break
        }
    }
    close(path)
    if (result < 0) {
        print "# cannot read test: " path
        failed = 1
    }
    if (!candidate) next
    for (key in alias) delete alias[key]
    comment = 0
    continuation = ""
    number = 0
    while ((result = (getline line < path)) > 0) {
        ++number
        # Skip ordinary code cheaply; only comments can span into directives.
        if (!comment && continuation == "" && line !~ /^[[:space:]]*#/ &&
            line !~ /\/\*/) continue
        line = continuation clean_line(line)
        continuation = ""
        if (line ~ /\\$/) {
            sub(/\\$/, "", line)
            continuation = line
            continue
        }
        sub(/^[[:space:]]+/, "", line)
        sub(/[[:space:]]+$/, "", line)
        directive_line(line, path, number)
    }
    close(path)
    if (result < 0) {
        print "# cannot read test: " path
        failed = 1
    }
}
END { exit failed }
