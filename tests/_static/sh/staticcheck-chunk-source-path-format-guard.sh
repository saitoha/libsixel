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
BEGIN {
    in_block = 0
    statement = ""
    start_line = 0
}
{
    line = $0
    code = ""
    i = 1
    while (i <= length(line)) {
        two = substr(line, i, 2)
        if (in_block) {
            if (two == "*/") {
                in_block = 0
                i += 2
                continue
            }
            i += 1
            continue
        }
        if (two == "/*") {
            in_block = 1
            i += 2
            continue
        }
        if (two == "//") {
            break
        }
        code = code substr(line, i, 1)
        i += 1
    }
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
