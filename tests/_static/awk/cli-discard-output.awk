# A named stdout capture must have a consumer beyond its redirection.
function inspect(    i, line, rest, name, target, errors) {
    for (name in refs) delete refs[name]
    for (name in captures) delete captures[name]
    target = 0
    # Most TAP files have no named converter capture. Reject them cheaply
    # before tokenizing variable references in the small candidate set.
    for (i = 1; i <= count; ++i) {
        line = lines[i]
        if (!index(line, "IMG2SIXEL_PATH") ||
            line ~ /^[[:space:]]*#/) continue
        if (match(line, /(^|[^0-9>])>[ \t]*"\$\{[A-Za-z_][A-Za-z_0-9]*\}"/)) {
            name = substr(line, RSTART, RLENGTH)
            sub(/^.*\$\{/, "", name)
            sub(/\}"$/, "", name)
            captures[name] = i
            target = 1
        }
    }
    if (!target) return 0
    for (i = 1; i <= count; ++i) {
        line = lines[i]
        if (line ~ /^[[:space:]]*#/) continue
        rest = line
        while (match(rest, /\$\{?[A-Za-z_][A-Za-z_0-9]*/)) {
            name = substr(rest, RSTART, RLENGTH)
            sub(/^\$\{?/, "", name)
            ++refs[name]
            rest = substr(rest, RSTART + RLENGTH)
        }
    }
    for (name in captures) {
        if (refs[name] == 1) {
            if (!control) print file ":" captures[name] ": unobserved converter capture; use -o/dev/null"
            ++errors
        }
    }
    return errors
}
function specimen(text) {
    count = split(text, lines, "\n")
    return inspect()
}
BEGIN {
    control = 1
    command = "${SIXEL_RUNTIME-} \"${IMG2SIXEL_PATH}\" input"
    if (!specimen(command " >\"${output}\"") ||
        specimen(command " -o/dev/null") ||
        specimen(command " >\"${output}\"\ncmp \"${output}\" expected") ||
        specimen(command " >\"${output}\"\ntest -s \"$output\"") ||
        specimen(command " 2>\"${diagnostic}\"")) {
        print "converter capture controls failed"
        failed = 1
        exit 1
    }
    control = count = 0
}
FNR == 1 {
    if (count && inspect()) failed = 1
    file = FILENAME
    count = 0
}
{
    line = $0
    while (sub(/\\$/, "", line)) {
        if ((getline continued) <= 0) break
        line = line continued
    }
    lines[++count] = line
}
END {
    if (!control && count && inspect()) failed = 1
    exit failed
}
