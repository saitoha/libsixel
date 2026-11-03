# Translate Meson's GCC response syntax into the SDK's paired object layout.
# Arguments are paths only. Nested @files and options are not object inputs.
function fail(message) {
    print "cosmoar-meson: " message
    exit 1
}
function quote(path,    i, c, result) {
    if (path !~ /[\\"]/) return "\"" path "\""
    result = "\""
    for (i = 1; i <= length(path); ++i) {
        c = substr(path, i, 1)
        if (c == "\\" || c == "\"") result = result "\\"
        result = result c
    }
    return result "\""
}
function emit(path,    directory, name) {
    if (path == "" || path ~ /^[-@]/ || path ~ /[\r\n]/)
        fail("invalid object path")
    name = path
    sub(/^.*\//, "", name)
    directory = substr(path, 1, length(path) - length(name))
    print quote(path) > x86
    print quote(directory ".aarch64/" name) > arm
    ++count
}
function parse(line,    i, j, n, size, parts, part, c, token, delimiter,
        escaped, active) {
    token = delimiter = ""
    escaped = active = 0
    # Index short pieces instead of rescanning a huge UTF-8 record for every
    # character. Preserve each separating space, including inside quotes.
    n = split(line, parts, /[ ]/)
    for (j = 1; j <= n; ++j) {
        part = parts[j]
        if (delimiter == "" && !escaped && part !~ /[\\"\047\t\r]/) {
            token = token part
            active = active || part != ""
            if (j < n) {
                if (active) emit(token)
                token = ""
                active = 0
            }
            continue
        }
        if (j < n) part = part " "
        size = length(part)
        for (i = 1; i <= size; ++i) {
            c = substr(part, i, 1)
            if (escaped) {
                token = token c
                escaped = 0
            } else if (c == "\\") {
                escaped = active = 1
            } else if (delimiter != "") {
                if (c == delimiter) delimiter = ""
                else token = token c
            } else if (c == "\"" || c == "\047") {
                delimiter = c
                active = 1
            } else if (c ~ /[ \t\r]/) {
                if (active) emit(token)
                token = ""
                active = 0
            } else {
                token = token c
                active = 1
            }
        }
    }
    if (escaped || delimiter != "") fail("unterminated response quoting")
    if (active) emit(token)
}
substr($0, 1, 1) == "D" { emit(substr($0, 2)); next }
{
    path = substr($0, 2)
    while ((status = getline line < path) > 0) parse(line)
    close(path)
    if (status < 0) fail("cannot read response file: " path)
}
END {
    if (!count) exit 1
}
