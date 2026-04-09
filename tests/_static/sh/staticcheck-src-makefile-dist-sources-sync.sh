#!/bin/sh
# Emit TAP for src/Makefile.am and src/Makefile.in source-list sync.

set -eu

src_root=$1
am_file=$src_root/src/Makefile.am
in_file=$src_root/src/Makefile.in

echo "1..1"

if test ! -f "$am_file"; then
    echo "not ok 1 - src Makefile source lists stay synchronized"
    echo "# missing file: $am_file"
    exit 1
fi

if test ! -f "$in_file"; then
    echo "not ok 1 - src Makefile source lists stay synchronized"
    echo "# missing file: $in_file"
    exit 1
fi

if missing_entries=$(awk '
function collect_tokens(line, arr, missing_only, tok) {
    while (match(line, /\$\(srcdir\)\/[A-Za-z0-9_.-]+/)) {
        tok = substr(line, RSTART, RLENGTH)
        if (missing_only == 0) {
            arr[tok] = 1
        } else if (!(tok in arr) && !(tok in reported)) {
            print tok
            reported[tok] = 1
            missing = 1
        }
        line = substr(line, RSTART + RLENGTH)
    }
}
FNR == 1 {
    file_index++
}
{
    if (file_index == 1) {
        collect_tokens($0, in_tokens, 0)
    } else if (file_index == 2) {
        collect_tokens($0, in_tokens, 1)
    }
}
END {
    if (missing == 1) {
        exit 1
    }
}
' "$in_file" "$am_file"); then
    :
else
    echo "not ok 1 - src Makefile source lists stay synchronized"
    printf '%s\n' "$missing_entries" |
        sed 's/^/# missing in src\/Makefile.in: /'
    exit 1
fi

echo "ok 1 - src Makefile source lists stay synchronized"
