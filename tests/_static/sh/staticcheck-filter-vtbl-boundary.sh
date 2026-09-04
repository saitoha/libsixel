#!/bin/sh
# Emit TAP for concrete-filter construction and execution boundaries.

set -eu

src_root=${1:-}

echo "1..3"

if test -z "$src_root"; then
    echo "not ok 1 - filter headers hide direct execution helpers"
    echo "# src_root argument is required"
    echo "not ok 2 - filter execution stays behind the vtbl"
    echo "# src_root argument is required"
    echo "not ok 3 - filter construction stays behind the factory"
    echo "# src_root argument is required"
    exit 1
fi

if test ! -d "$src_root/src"; then
    echo "not ok 1 - filter headers hide direct execution helpers"
    echo "# missing source directory: $src_root/src"
    echo "not ok 2 - filter execution stays behind the vtbl"
    echo "# missing source directory: $src_root/src"
    echo "not ok 3 - filter construction stays behind the factory"
    echo "# missing source directory: $src_root/src"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-filter-vtbl-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

header_violations=$tmpdir/header-execution.txt
execution_violations=$tmpdir/direct-execution.txt
constructor_violations=$tmpdir/direct-construction.txt
failed=0

find "$src_root/src" -maxdepth 1 -type f -name 'filter-*.h' \
    -exec awk '
/sixel_filter_[A-Za-z0-9_]+_(apply|frame)[[:space:]]*\(/ {
    print FILENAME ":" FNR ":" $0
}
' {} + > "$header_violations"

if test -s "$header_violations"; then
    echo "not ok 1 - filter headers hide direct execution helpers"
    sed 's/^/# exposed execution helper: /' "$header_violations"
    failed=1
else
    echo "ok 1 - filter headers hide direct execution helpers"
fi

find "$src_root/src" -maxdepth 1 -type f -name '*.c' -exec awk '
function owner_path(line, suffix, component, start) {
    start = index(line, "sixel_filter_")
    component = substr(line, start + length("sixel_filter_"))
    sub(/[[:space:]]*\(.*/, "", component)
    sub(suffix "$", "", component)
    if (suffix == "_frame") {
        sub(/_(copy|create)$/, "", component)
    }
    if (component == "1d_eytzinger") {
        component = "eytzinger"
    }
    gsub(/_/, "-", component)
    return "/filter-" component ".c"
}
/sixel_filter_[A-Za-z0-9_]+_(apply|frame)[[:space:]]*\(/ {
    suffix = $0 ~ /_frame[[:space:]]*\(/ ? "_frame" : "_apply"
    owner = owner_path($0, suffix)
    if (index(FILENAME, owner) == 0) {
        print FILENAME ":" FNR ":" $0
    }
}
' {} + > "$execution_violations"

if test -s "$execution_violations"; then
    echo "not ok 2 - filter execution stays behind the vtbl"
    sed 's/^/# direct execution: /' "$execution_violations"
    failed=1
else
    echo "ok 2 - filter execution stays behind the vtbl"
fi

find "$src_root/src" -maxdepth 1 -type f -name '*.c' -exec awk '
function owner_path(line, component, start) {
    start = index(line, "sixel_filter_")
    component = substr(line, start + length("sixel_filter_"))
    sub(/_init[[:space:]]*\(.*/, "", component)
    if (component == "1d_eytzinger") {
        component = "eytzinger"
    }
    gsub(/_/, "-", component)
    return "/filter-" component ".c"
}
/sixel_filter_[A-Za-z0-9_]+_init[[:space:]]*\(/ {
    owner = owner_path($0)
    if (FILENAME !~ /\/filter-factory\.c$/ &&
        index(FILENAME, owner) == 0) {
        print FILENAME ":" FNR ":" $0
    }
}
' {} + > "$constructor_violations"

if test -s "$constructor_violations"; then
    echo "not ok 3 - filter construction stays behind the factory"
    sed 's/^/# direct construction: /' "$constructor_violations"
    failed=1
else
    echo "ok 3 - filter construction stays behind the factory"
fi

exit "$failed"
