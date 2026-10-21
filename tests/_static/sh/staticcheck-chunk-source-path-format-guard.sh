#!/bin/sh
# Emit TAP for banning direct chunk source_path() calls in format arguments.

set -eu

src_root=${1:-}

echo "1..1"

if test -z "$src_root"; then
    echo "not ok 1 - chunk source_path is bound before format output"
    echo "# src_root argument is required"
    exit 1
fi

matches=$(mktemp "${TMPDIR:-/tmp}/libsixel-chunk-source-path-format-XXXXXX")
trap 'rm -f "$matches"' EXIT HUP INT TERM

find "$src_root/src" "$src_root/include" "$src_root/tests" \
    "$src_root/converters" "$src_root/assessment" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.inc.c' \) \
    -exec awk '
# Skip complete comment/code spans without rescanning each character.
function strip_comments(line, code, block_start, line_start, block_end) {
    code = ""
    while (line != "") {
        if (in_block) {
            block_end = index(line, "*/")
            if (!block_end) return code
            line = substr(line, block_end + 2)
            in_block = 0
            continue
        }
        block_start = index(line, "/*")
        line_start = index(line, "//")
        if (line_start && (!block_start || line_start < block_start)) {
            return code substr(line, 1, line_start - 1)
        }
        if (!block_start) return code line
        code = code substr(line, 1, block_start - 1)
        line = substr(line, block_start + 2)
        in_block = 1
    }
    return code
}
BEGIN {
    in_block = 0
    statement = ""
    start_line = 0
}
{
    line = $0
    code = strip_comments(line)
    if (code == "") {
        next
    }
    if (statement == "") {
        start_line = FNR
    }
    statement = statement " " code
    if (code ~ /;/) {
        if (statement ~ /(sixel_compat_snprintf|snprintf|fprintf|printf)[^;]*sixel_chunk_get_source_path[[:space:]]*\(/) {
            printf "%s:%d:%s\n", FILENAME, start_line, statement
        }
        statement = ""
        start_line = 0
    }
}
' {} + > "$matches"

if test -s "$matches"; then
    echo "not ok 1 - chunk source_path is bound before format output"
    sed 's/^/# direct source_path format argument: /' "$matches"
    exit 1
fi

echo "ok 1 - chunk source_path is bound before format output"
