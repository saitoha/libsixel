#!/bin/sh
# Emit TAP for enforcing single-observation test plans.

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

(
    cd "$tests_root"
    find . \
        \( \
            -path './.perl-test-venv' -o \
            -path './.php-test-venv' -o \
            -path './.python-test-venv' -o \
            -path './.ruby-test-venv' -o \
            -path './_artifacts' -o \
            -path './data' \
        \) -prune -o \
        -type f \( \
            -name '*.t' -o \
            -path './bindings/php/[0-9][0-9][0-9][0-9]_*.php' -o \
            -path './bindings/python/[0-9][0-9][0-9][0-9]_*.py' -o \
            -path './bindings/ruby/[0-9][0-9][0-9][0-9]_*.rb' -o \
            -path './bindings/perl/[0-9][0-9][0-9][0-9]_*.pl' \
        \) -print
) | LC_ALL=C sort > "$tmpfile"

if test ! -s "$tmpfile"; then
    echo "ok 1 # SKIP no test sources found"
    exit 0
fi

failed=0
while IFS= read -r relpath; do
    test -n "$relpath" || continue
    relpath=${relpath#./}
    file=$tests_root/$relpath
    if ! awk -v file="$relpath" '
BEGIN {
    bad = 0
}
{
    line = $0
    rest = line
    while (match(rest, /1\.\.[0-9]+/)) {
        plan = substr(rest, RSTART, RLENGTH)
        if (plan != "1..1") {
            if (!(plan == "1..0" && line ~ /[Ss][Kk][Ii][Pp]/)) {
                printf "# %s:%d: disallowed TAP plan %s (expected 1..1 or 1..0 # SKIP)\n",
                    file, NR, plan
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
                file, NR, count
            bad = 1
        }
        rest = substr(rest, RSTART + RLENGTH)
    }
}
END {
    exit bad ? 1 : 0
}
' "$file"; then
        failed=1
    fi
done < "$tmpfile"

if test "$failed" -ne 0; then
    echo "not ok 1 - tests declare only single-observation plans"
    exit 1
fi

echo "ok 1 - tests declare only single-observation plans"
