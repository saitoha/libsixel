#!/bin/sh
# Emit TAP for concrete-filter construction and execution boundaries.

set -eu

src_root=${1:-}

echo "1..5"

if test -z "$src_root"; then
    echo "not ok 1 - boundary scanner rejects representative bypasses"
    echo "# src_root argument is required"
    echo "not ok 2 - filter headers hide direct execution symbols"
    echo "# src_root argument is required"
    echo "not ok 3 - concrete execution stays in the owning filter"
    echo "# src_root argument is required"
    echo "not ok 4 - filter construction stays behind the factory"
    echo "# src_root argument is required"
    echo "not ok 5 - filter vtbl dispatch stays behind sixel_filter_run"
    echo "# src_root argument is required"
    exit 1
fi

scanner=$src_root/tests/_static/awk/filter-vtbl-boundary.awk
fixture_dir=$src_root/tests/_static/fixtures/filter-vtbl-boundary

if test ! -f "$scanner" || test ! -d "$fixture_dir" ||
        test ! -d "$src_root/src"; then
    echo "not ok 1 - boundary scanner rejects representative bypasses"
    echo "# missing scanner, fixtures, or source directory"
    echo "not ok 2 - filter headers hide direct execution symbols"
    echo "# missing scanner, fixtures, or source directory"
    echo "not ok 3 - concrete execution stays in the owning filter"
    echo "# missing scanner, fixtures, or source directory"
    echo "not ok 4 - filter construction stays behind the factory"
    echo "# missing scanner, fixtures, or source directory"
    echo "not ok 5 - filter vtbl dispatch stays behind sixel_filter_run"
    echo "# missing scanner, fixtures, or source directory"
    exit 1
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-filter-vtbl-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

fixture_report=$tmpdir/fixture-report.txt
source_report=$tmpdir/source-report.txt
header_violations=$tmpdir/header-violations.txt
execution_violations=$tmpdir/execution-violations.txt
construction_violations=$tmpdir/construction-violations.txt
dispatch_violations=$tmpdir/dispatch-violations.txt
failed=0

awk -f "$scanner" \
    "$fixture_dir/bad.h.in" \
    "$fixture_dir/bad.c.in" \
    "$fixture_dir/filter-sample.c.in" > "$fixture_report"

if awk -F '|' '
$1 == "header" { ++header }
$1 == "execution" { ++execution }
$1 == "construction" { ++construction }
$1 == "dispatch" { ++dispatch }
END {
    exit (header == 1 && execution == 2 && construction == 3 && dispatch == 1 ? 0 : 1)
}
' "$fixture_report"; then
    echo "ok 1 - boundary scanner rejects representative bypasses"
else
    echo "not ok 1 - boundary scanner rejects representative bypasses"
    sed 's/^/# scanner result: /' "$fixture_report"
    failed=1
fi

awk -f "$scanner" "$src_root"/src/filter-*.h \
    "$src_root"/src/*.c > "$source_report"

awk -F '|' '$1 == "header" {
    print $2 ":" $3 ":" $4
}' "$source_report" > "$header_violations"
awk -F '|' '$1 == "execution" {
    print $2 ":" $3 ":" $4
}' "$source_report" > "$execution_violations"
awk -F '|' '$1 == "construction" {
    print $2 ":" $3 ":" $4
}' "$source_report" > "$construction_violations"
awk -F '|' '$1 == "dispatch" {
    print $2 ":" $3 ":" $4
}' "$source_report" > "$dispatch_violations"

if test -s "$header_violations"; then
    echo "not ok 2 - filter headers hide direct execution symbols"
    sed 's/^/# /' "$header_violations"
    failed=1
else
    echo "ok 2 - filter headers hide direct execution symbols"
fi

if test -s "$execution_violations"; then
    echo "not ok 3 - concrete execution stays in the owning filter"
    sed 's/^/# /' "$execution_violations"
    failed=1
else
    echo "ok 3 - concrete execution stays in the owning filter"
fi

if test -s "$construction_violations"; then
    echo "not ok 4 - filter construction stays behind the factory"
    sed 's/^/# /' "$construction_violations"
    failed=1
else
    echo "ok 4 - filter construction stays behind the factory"
fi

if test -s "$dispatch_violations"; then
    echo "not ok 5 - filter vtbl dispatch stays behind sixel_filter_run"
    sed 's/^/# /' "$dispatch_violations"
    failed=1
else
    echo "ok 5 - filter vtbl dispatch stays behind sixel_filter_run"
fi

exit "$failed"
