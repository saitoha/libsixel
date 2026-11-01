#!/bin/sh
# Emit TAP for enforcing single-observation test plans.
# Policy: docs/testing/guide.md

set -eu

src_root=$1
tests_root=$src_root/tests

echo "1..1"

if test ! -d "$tests_root"; then
    echo "not ok 1 - test plan policy"
    echo "# tests directory not found: $tests_root"
    exit 1
fi

tmpfile=$(mktemp "${TMPDIR:-/tmp}/libsixel-test-plan-sources-XXXXXX")
cleanup() {
    rm -f "$tmpfile"
}
trap cleanup EXIT HUP INT TERM

sh "$src_root/build-aux/read-check-test-list.sh" tests "$tests_root" \
    include newline > "$tmpfile"
# Numbered source-only shell contracts belong to staticcheck, but retain
# the same one-observation TAP contract as runtime wrappers.
(CDPATH='' cd "$tests_root" && printf '%s\n' */*/[0-9][0-9][0-9][0-9]_*.sh) |
    awk 'index($0, "*") == 0' >> "$tmpfile"

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no test sources found"
    exit 0
fi

# Read the sorted inventory in one awk process. Explicit getline keeps empty
# .t files observable, and close prevents descriptor growth with suite size.
failed=0
awk -v tests_root="$tests_root" '
{
    file = $0
    sub(/^\.\//, "", file)
    path = tests_root "/" file
    normal_plan_count = 0
    line_number = 0
    while ((read_status = (getline line < path)) > 0) {
        line_number++
        rest = line
        while (match(rest, /1\.\.[0-9]+/)) {
            plan = substr(rest, RSTART, RLENGTH)
            if (plan == "1..1") {
                normal_plan_count += 1
            }
            if (plan != "1..1") {
                if (!(plan == "1..0" && line ~ /[Ss][Kk][Ii][Pp]/)) {
                    printf "# %s:%d: disallowed TAP plan %s (expected 1..1 or 1..0 # SKIP)\n",
                        file, line_number, plan
                    bad = 1
                }
            }
            rest = substr(rest, RSTART + RLENGTH)
        }

        rest = line
        while (match(rest, /plan[[:space:]]*\(?[[:space:]]*tests[[:space:]]*=>[[:space:]]*[0-9]+/)) {
            expr = substr(rest, RSTART, RLENGTH)
            count = expr
            sub(/.*=>[[:space:]]*/, "", count)
            sub(/[^0-9].*/, "", count)
            if (count != "1") {
                printf "# %s:%d: disallowed Perl plan tests => %s (expected tests => 1)\n",
                    file, line_number, count
                bad = 1
            }
            rest = substr(rest, RSTART + RLENGTH)
        }
    }
    close(path)
    if (read_status < 0) {
        print "# " file ": cannot read test source"
        bad = 1
    }
    if (file ~ /\.t$/ && normal_plan_count != 1) {
        printf "# %s: found %d normal TAP plans (expected exactly one)\n", \
            file, normal_plan_count
        bad = 1
    }
}
END { exit bad ? 1 : 0 }
' "$tmpfile" || failed=1

if test "$failed" -ne 0; then
    echo "not ok 1 - tests declare only single-observation plans"
    exit 1
fi

echo "ok 1 - tests declare only single-observation plans"
