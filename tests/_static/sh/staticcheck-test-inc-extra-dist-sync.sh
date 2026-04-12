#!/bin/sh
# Emit TAP for tests include-based .inc.c and EXTRA_DIST synchronization.

set -eu

src_root=$1
tests_root=$src_root/tests
am_file=$tests_root/Makefile.am
in_file=$tests_root/Makefile.in
tmpdir=
includes_file=
am_dist_file=
in_dist_file=
all_dist_file=
missing_paths_file=
fail=0
missing_in_am=
missing_in_in=
extra_in_am=
extra_in_in=
missing_paths=

echo "1..1"

if test ! -d "$tests_root"; then
    echo "not ok 1 - tests inc EXTRA_DIST entries stay synchronized"
    echo "# missing directory: $tests_root"
    exit 1
fi

if test ! -f "$am_file"; then
    echo "not ok 1 - tests inc EXTRA_DIST entries stay synchronized"
    echo "# missing file: $am_file"
    exit 1
fi

if test ! -f "$in_file"; then
    echo "not ok 1 - tests inc EXTRA_DIST entries stay synchronized"
    echo "# missing file: $in_file"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-tests-inc-sync-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

includes_file=$tmpdir/includes.txt
am_dist_file=$tmpdir/am-dist.txt
in_dist_file=$tmpdir/in-dist.txt
all_dist_file=$tmpdir/all-dist.txt
missing_paths_file=$tmpdir/missing-paths.txt

find "$tests_root" -type f -name '*.c' | LC_ALL=C sort | while IFS= read -r c_file
do
    awk '
    {
        line = $0
        while (match(line, /#include[[:space:]]+"tests\/[A-Za-z0-9_.\/-]+\.inc\.c"/)) {
            token = substr(line, RSTART, RLENGTH)
            sub(/^#include[[:space:]]+"/, "", token)
            sub(/"$/, "", token)
            sub(/^tests\//, "", token)
            print token
            line = substr(line, RSTART + RLENGTH)
        }
    }
    ' "$c_file"
done | LC_ALL=C sort -u > "$includes_file"

awk '
BEGIN {
    in_extra_dist = 0
}
{
    if (in_extra_dist == 0 && $0 ~ /^EXTRA_DIST[[:space:]]*=/) {
        in_extra_dist = 1
    }
    if (in_extra_dist == 0) {
        next
    }
    line = $0
    sub(/^[[:space:]]*EXTRA_DIST[[:space:]]*=[[:space:]]*/, "", line)
    gsub(/\\/, " ", line)
    n = split(line, parts, /[[:space:]]+/)
    for (i = 1; i <= n; ++i) {
        token = parts[i]
        if (token ~ /^[A-Za-z0-9_.\/-]+\.inc\.c$/) {
            print token
        }
    }
    if ($0 !~ /\\[[:space:]]*$/) {
        in_extra_dist = 0
    }
}
' "$am_file" | LC_ALL=C sort -u > "$am_dist_file"

awk '
BEGIN {
    in_extra_dist = 0
}
{
    if (in_extra_dist == 0 && $0 ~ /^EXTRA_DIST[[:space:]]*=/) {
        in_extra_dist = 1
    }
    if (in_extra_dist == 0) {
        next
    }
    line = $0
    sub(/^[[:space:]]*EXTRA_DIST[[:space:]]*=[[:space:]]*/, "", line)
    gsub(/\\/, " ", line)
    n = split(line, parts, /[[:space:]]+/)
    for (i = 1; i <= n; ++i) {
        token = parts[i]
        if (token ~ /^[A-Za-z0-9_.\/-]+\.inc\.c$/) {
            print token
        }
    }
    if ($0 !~ /\\[[:space:]]*$/) {
        in_extra_dist = 0
    }
}
' "$in_file" | LC_ALL=C sort -u > "$in_dist_file"

cat "$am_dist_file" "$in_dist_file" | LC_ALL=C sort -u > "$all_dist_file"
: > "$missing_paths_file"
while IFS= read -r rel_path
do
    test -n "$rel_path" || continue
    test -f "$tests_root/$rel_path" || printf '%s\n' "$rel_path" >> "$missing_paths_file"
done < "$all_dist_file"
missing_paths=$(cat "$missing_paths_file")
if test -n "$missing_paths"; then
    fail=1
fi

if missing_in_am=$(awk '
NR == FNR {
    have[$0] = 1
    next
}
!($0 in have) {
    print $0
    missing = 1
}
END {
    exit missing ? 1 : 0
}
' "$am_dist_file" "$includes_file"); then
    :
else
    fail=1
fi

if missing_in_in=$(awk '
NR == FNR {
    have[$0] = 1
    next
}
!($0 in have) {
    print $0
    missing = 1
}
END {
    exit missing ? 1 : 0
}
' "$in_dist_file" "$includes_file"); then
    :
else
    fail=1
fi

if extra_in_am=$(awk '
NR == FNR {
    include_set[$0] = 1
    next
}
!($0 in include_set) {
    print $0
    extra = 1
}
END {
    exit extra ? 1 : 0
}
' "$includes_file" "$am_dist_file"); then
    :
else
    fail=1
fi

if extra_in_in=$(awk '
NR == FNR {
    include_set[$0] = 1
    next
}
!($0 in include_set) {
    print $0
    extra = 1
}
END {
    exit extra ? 1 : 0
}
' "$includes_file" "$in_dist_file"); then
    :
else
    fail=1
fi

if test "$fail" -ne 0; then
    echo "not ok 1 - tests inc EXTRA_DIST entries stay synchronized"
    if test -n "$missing_in_am"; then
        printf '%s\n' "$missing_in_am" |
            sed 's/^/# missing in tests\/Makefile.am EXTRA_DIST: /'
    fi
    if test -n "$missing_in_in"; then
        printf '%s\n' "$missing_in_in" |
            sed 's/^/# missing in tests\/Makefile.in EXTRA_DIST: /'
    fi
    if test -n "$extra_in_am"; then
        printf '%s\n' "$extra_in_am" |
            sed 's/^/# extra in tests\/Makefile.am EXTRA_DIST: /'
    fi
    if test -n "$extra_in_in"; then
        printf '%s\n' "$extra_in_in" |
            sed 's/^/# extra in tests\/Makefile.in EXTRA_DIST: /'
    fi
    if test -n "$missing_paths"; then
        printf '%s\n' "$missing_paths" |
            sed 's/^/# missing file in tests tree: /'
    fi
    exit 1
fi

echo "ok 1 - tests inc EXTRA_DIST entries stay synchronized"
