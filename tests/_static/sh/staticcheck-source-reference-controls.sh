#!/bin/sh
# Verify shared/scoped source references, placement errors and batch retention.
# Policy: docs/testing/staticcheck.md
set -eux

echo '1..1'
set -v
src_root=$1
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/libsixel-source-references-XXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM
mkdir -p "$tmpdir/root/tests" "$tmpdir/all" "$tmpdir/policy" "$tmpdir/plan"

# Build deliberately invalid markers without embedding them in this source:
# the production collector also audits the regression script itself.
awk -v root="$tmpdir/root/tests" '
BEGIN {
    policy="Policy" ": docs/"
    plan="Test-plan" ": docs/"
    role="Staticcheck" ": staticcheck-"
    print "# " policy "a.md" > (root "/first.t")
    print "# " plan "b.md" > (root "/first.t")
    print "# " role "control" > (root "/first.t")
    for (i=4; i<=20; i++) print "# padding" > (root "/first.t")
    print "# " policy "late.md" > (root "/first.t")
    print "// " policy "c.md" > (root "/second.c")
    print "// " plan "d.md" > (root "/second.c")
    print "// " policy "bad.md" > (root "/third.ps1")
    print "# " plan "e.md" > (root "/third.ps1")
    print "# " role "ignored" > (root "/third.ps1")
    print "# " policy "a.md" > (root "/empty.sh")
    printf "" > (root "/empty.t")
}'
sh "$src_root/tests/_static/sh/collect-source-references.sh" \
    "$tmpdir/root" "$tmpdir/all" || {
    echo 'not ok 1 - shared reference collection failed'
    exit 0
}
sh "$src_root/tests/_static/sh/collect-source-references.sh" \
    "$tmpdir/root" "$tmpdir/policy" policy || {
    echo 'not ok 1 - scoped policy collection failed'
    exit 0
}
sh "$src_root/tests/_static/sh/collect-source-references.sh" \
    "$tmpdir/root" "$tmpdir/plan" test-plan || {
    echo 'not ok 1 - scoped plan collection failed'
    exit 0
}
cmp "$tmpdir/all/policy-links" "$tmpdir/policy/policy-links" || {
    echo 'not ok 1 - shared policy inventory differs from focused collection'
    exit 0
}
cmp "$tmpdir/all/test-plan-links" "$tmpdir/plan/test-plan-links" || {
    echo 'not ok 1 - shared plan inventory differs from focused collection'
    exit 0
}
LC_ALL=C sort "$tmpdir/all/policy-links" > "$tmpdir/actual"
printf '%s\n' 'docs/a.md|tests/empty.sh' 'docs/a.md|tests/first.t' \
    'docs/bad.md|tests/third.ps1' 'docs/c.md|tests/second.c' \
    'docs/late.md|tests/first.t' > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/actual" || {
    echo 'not ok 1 - policy inventory lost source identities or late markers'
    exit 0
}
LC_ALL=C sort "$tmpdir/all/policy-errors" > "$tmpdir/actual"
printf '%s\n' \
    'tests/first.t: Policy reference must be a source comment within the first 20 lines' \
    'tests/third.ps1: Policy reference must be a source comment within the first 20 lines' \
    > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/actual" || {
    echo 'not ok 1 - placement or PowerShell comment errors were lost'
    exit 0
}
LC_ALL=C sort "$tmpdir/all/test-plan-links" > "$tmpdir/actual"
printf '%s\n' 'docs/b.md|tests/first.t' 'docs/d.md|tests/second.c' \
    'docs/e.md|tests/third.ps1' > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/actual" || {
    echo 'not ok 1 - test plan references changed'
    exit 0
}
test ! -s "$tmpdir/all/test-plan-errors" || {
    echo 'not ok 1 - valid test plan references were rejected'
    exit 0
}
printf '%s\n' "staticcheck-control|$tmpdir/root/tests/first.t" \
    > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/all/source-contracts" || {
    echo 'not ok 1 - role registration must select only shell TAP files'
    exit 0
}

# Source enumeration batches its arguments. Successive awk processes must append,
# not discard records produced by the earlier batch.
: > "$tmpdir/all/policy-links"
awk -v src_root="$tmpdir/root/" -v output_root="$tmpdir/all" -v scope=policy \
    -f "$src_root/tests/_static/awk/source-references.awk" \
    "$tmpdir/root/tests/second.c"
awk -v src_root="$tmpdir/root/" -v output_root="$tmpdir/all" -v scope=policy \
    -f "$src_root/tests/_static/awk/source-references.awk" \
    "$tmpdir/root/tests/empty.sh"
printf '%s\n' 'docs/c.md|tests/second.c' 'docs/a.md|tests/empty.sh' \
    > "$tmpdir/expected"
cmp "$tmpdir/expected" "$tmpdir/all/policy-links" || {
    echo 'not ok 1 - later batches erased earlier reference records'
    exit 0
}
awk 'BEGIN { print "# Staticcheck" ": invalid" }' \
    >> "$tmpdir/root/tests/first.t"
status=0
sh "$src_root/tests/_static/sh/collect-source-references.sh" \
    "$tmpdir/root" "$tmpdir/all" >/dev/null 2>&1 || status=$?
test "$status" -ne 0 || {
    echo 'not ok 1 - invalid role registrations must fail collection'
    exit 0
}
echo 'ok 1 - shared source references preserve scoped contracts and errors'
