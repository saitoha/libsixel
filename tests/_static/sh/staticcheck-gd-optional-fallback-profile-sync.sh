#!/bin/sh
# Emit TAP for gd optional fallback profile script and TAP tests staying synchronized.

set -eu

echo "1..1"

src_root=$1
gd_tests_dir=$src_root/tests/loader/gd
profile_script=$src_root/tests/_static/sh/run-loader-gd-optional-fallback-profile.sh

if test ! -d "$gd_tests_dir" || test ! -f "$profile_script"; then
    echo "ok 1 # SKIP missing gd test directory or profile script"
    exit 0
fi

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-staticcheck-gd-optional-profile-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

expected=$tmpdir/expected.txt
actual=$tmpdir/actual.txt
missing=$tmpdir/missing.txt
extra=$tmpdir/extra.txt
status=0

find "$gd_tests_dir" -type f -name '*.t' -print | awk -F '/' '
/loader_gd_.*unsupported(_stdin)?_fallback_.*\.t$/ {
    print $NF
}
' | LC_ALL=C sort -u > "$expected"

awk '
{
    line = $0
    while (match(line, /[0-9][0-9][0-9][0-9]_[^[:space:]]+\.t/)) {
        token = substr(line, RSTART, RLENGTH)
        print token
        line = substr(line, RSTART + RLENGTH)
    }
}
' "$profile_script" | LC_ALL=C sort -u > "$actual"

comm -23 "$expected" "$actual" > "$missing"
comm -13 "$expected" "$actual" > "$extra"

if test -s "$missing"; then
    status=1
fi
if test -s "$extra"; then
    status=1
fi

if test "$status" -eq 0; then
    echo "ok 1 - gd optional fallback profile list stays synchronized"
    exit 0
fi

echo "not ok 1 - gd optional fallback profile list stays synchronized"
while IFS= read -r test_name; do
    test -n "$test_name" || continue
    echo "# missing from profile list: $test_name"
done < "$missing"
while IFS= read -r test_name; do
    test -n "$test_name" || continue
    echo "# extra in profile list: $test_name"
done < "$extra"
exit 1
