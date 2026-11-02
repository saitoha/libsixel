# Select from the suite's immutable test-source inventory without rescanning it.
# Shell-glob stars match slashes here, as in the inventory's path exclusions.
function glob(pattern,    i, ch, result, bracket) {
    result = "^"
    for (i = 1; i <= length(pattern); i++) {
        ch = substr(pattern, i, 1)
        if (ch == "[") {
            bracket = index(substr(pattern, i + 1), "]")
            if (bracket) {
                ch = substr(pattern, i, bracket + 1)
                sub(/^\[!/, "[^", ch)
                result = result ch; i += bracket; continue
            }
        }
        if (ch == "*") result = result ".*"
        else if (ch == "?") result = result "."
        else if (index("[].()+^$|{}\\", ch)) result = result "\\" ch
        else result = result ch
    }
    return result "$"
}
function matches(value, patterns, count,    i) {
    for (i = 1; i <= count; i++) if (value ~ patterns[i]) return 1
    return 0
}
function compile(text, patterns,    n, words, i, count) {
    n = split(text, words, "\n")
    for (i = 1; i <= n; i++) if (words[i] != "") patterns[++count] = glob(words[i])
    return count
}
BEGIN {
    name_count = compile(ENVIRON["SIXEL_SCAN_NAMES"], name_patterns)
    exclude_count = compile(ENVIRON["SIXEL_SCAN_EXCLUDES"], exclude_patterns)
    gsub(/\\/, "/", absolute); gsub(/\\/, "/", requested)
    path_count = compile(ENVIRON["SIXEL_SCAN_PATHS"], path_patterns)
}
{ gsub(/\\/, "/", $0) }
index($0, absolute "/") == 1 {
    suffix = substr($0, length(absolute) + 2)
    depth = split(suffix, parts, "/")
    if (max_depth >= 0 && depth > (max_depth > 1 ? max_depth : 1)) next
    name = parts[depth]
    if (name_count && !matches(name, name_patterns, name_count)) next
    if (matches(name, exclude_patterns, exclude_count)) next
    path = requested "/" suffix
    excluded = 0
    ancestor = path
    while (length(ancestor) > length(requested)) {
        if (matches(ancestor, path_patterns, path_count)) { excluded = 1; break }
        sub(/\/[^\/]*$/, "", ancestor)
    }
    if (!excluded) {
        # Some BSD awks truncate NUL inside a string; emit it directly.
        if (nul) printf "%s%c", path, 0
        else print path
    }
}
