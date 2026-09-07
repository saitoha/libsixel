#!/bin/sh
# Emit TAP for enforcing the for/until ban in shell TAP test scripts.
# Policy: docs/testing/guide.md

set -eu

src_root=$1
tests_root=$src_root/tests

echo "1..1"

test -d "$tests_root" || {
    echo "not ok 1 - shell TAP tests avoid for and until loops"
    echo "# tests directory not found: $tests_root"
    exit 1
}

find "$tests_root" \
    \( -path "$tests_root/_artifacts" -o -path "$tests_root/data" \) \
        -prune -o \
    -type f -name '*.t' -exec awk '
FNR == 1 {
    heredoc_delimiter = ""
}
heredoc_delimiter != "" {
    heredoc_line = $0
    sub(/^\t+/, "", heredoc_line)
    if (heredoc_line == heredoc_delimiter) {
        heredoc_delimiter = ""
    }
    next
}
{
    if ($0 ~ /^[[:space:]]*(for|until)[[:space:]]/) {
        printf "# %s:%d: for and until loops are not allowed\n", \
            FILENAME, FNR
        failed = 1
    }
    if (match($0,
              /<<-?[[:space:]]*[\047\042]?[A-Za-z_][A-Za-z0-9_]*[\047\042]?/)) {
        heredoc_delimiter = substr($0, RSTART, RLENGTH)
        sub(/^<<-?[[:space:]]*/, "", heredoc_delimiter)
        gsub(/[\047\042]/, "", heredoc_delimiter)
    }
}
END {
    exit failed != 0 ? 1 : 0
}
' {} + || {
    echo "not ok 1 - shell TAP tests avoid for and until loops"
    exit 1
}

echo "ok 1 - shell TAP tests avoid for and until loops"
exit 0
