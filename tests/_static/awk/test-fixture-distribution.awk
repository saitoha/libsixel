# Resolve literal Automake distribution paths without invoking Make or a compiler.
function assignment(line,    name, op) {
    sub(/[[:space:]]*#.*/, "", line)
    if (line !~ /^[A-Za-z_][A-Za-z0-9_]*[[:space:]]*[+:?]?=/) return
    name = line
    sub(/[[:space:]]*[+:?]?=.*/, "", name)
    op = substr(line, length(name) + 1)
    sub(/^[[:space:]]*/, "", op)
    sub(/^[^=]*=[[:space:]]*/, "", line)
    if (op ~ /^\+=/) value[name] = value[name] " " line
    else if (op !~ /^\?=/ || !(name in value)) value[name] = line
}
# Read only requested variables: joining the runner's huge C source/macro
# inventories would add quadratic string work unrelated to fixture coverage.
function read_value(name,    line, pending, selected, status) {
    while ((status = getline line < makefile) > 0) {
        if (!selected)
            selected = line ~ ("^" name "[[:space:]]*[+:?]?=")
        if (!selected) continue
        line = pending line
        if (sub(/\\$/, "", line)) { pending = line; continue }
        assignment(line)
        pending = ""
        selected = 0
    }
    close(makefile)
    if (status < 0 || !(name in value)) invalid = 1
}
function expand(text, depth,    name, replacement) {
    if (depth > 20) { invalid = 1; return "" }
    while (match(text, /\$\([A-Za-z_][A-Za-z0-9_]*\)/)) {
        name = substr(text, RSTART + 2, RLENGTH - 3)
        prefix[depth] = substr(text, 1, RSTART - 1)
        suffix[depth] = substr(text, RSTART + RLENGTH)
        if (!(name in value)) read_value(name)
        replacement = expand(value[name], depth + 1)
        text = prefix[depth] replacement suffix[depth]
    }
    if (text ~ /\$/) invalid = 1
    return text
}
function covered(path,    parent) {
    parent = path
    while (parent != "") {
        if (parent in distributed) return 1
        if (!sub(/\/[^\/]*$/, "", parent)) break
    }
    return 0
}
BEGIN {
    # An ancestor directory covers its descendants, but not a sibling prefix.
    distributed["data"] = 1
    distributed["tools/compare/data/octet.bin"] = 1
    if (!covered("data/nested/file") ||
        !covered("tools/compare/data/octet.bin") ||
        covered("tools/compare/data/missing") || covered("database/file"))
        invalid = 1
    delete distributed["data"]
    delete distributed["tools/compare/data/octet.bin"]
    assignment("FIXTURES = data")
    assignment("EXTRA_DIST = $(FIXTURES)")
    assignment("EXTRA_DIST += tools/compare/data/octet.bin # comment")
    if (expand(value["EXTRA_DIST"], 0) != "data tools/compare/data/octet.bin") invalid = 1
    for (key in value) delete value[key]
}

{
    if (!loaded) {
        read_value("EXTRA_DIST")
        count = split(expand(value["EXTRA_DIST"], 0), entries, /[[:space:]]+/)
        for (i = 1; i <= count; i++) distributed[entries[i]] = 1
        loaded = 1
    }
    path = substr($0, length(root) + 2)
    if (path !~ /(^|\/)data\//) next
    if (!covered(path)) {
        print "tests/" path ": fixture missing from EXTRA_DIST"
        failed = 1
    }
    fixtures++
}
END {
    if (invalid) print "fixture distribution controls or Make variable expansion failed"
    if (!fixtures) print "no test fixture inventory found"
    exit (invalid || failed || !fixtures)
}
