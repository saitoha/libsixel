#!/bin/sh
# Policy: docs/testing/staticcheck.md
# Policy: docs/ci/design.md
set -eux
echo '1..1'
set -v
src_root=$1
awk -f "$src_root/tests/_static/awk/ci-vm-result.awk" \
    "$src_root/.github/actions/ci-steps/action.yml" >&2 || {
    echo 'not ok 1 - VM build result handoff in workflow'
    exit 0
}
# This file is the observed host/guest handoff, not an incidental test log.
result=$(mktemp "${TMPDIR:-/tmp}/libsixel-vm-result-XXXXXX")
trap 'rm -f "$result"' EXIT HUP INT TERM
sh "$src_root/tools/ci-vm-result.sh" capture "$result" <<'BUILD' || {
exit 23
exit 0
BUILD
    echo 'not ok 1 - guest failure prevented artifact copyback'
    exit 0
}
status=0
sh "$src_root/tools/ci-vm-result.sh" check "$result" >&2 || status=$?
test "$status" -eq 23 || {
    echo 'not ok 1 - guest failure did not reach host'
    exit 0
}
# A failed intermediate command must stop the build as well.
sh "$src_root/tools/ci-vm-result.sh" capture "$result" <<'BUILD' || {
false
exit 0
BUILD
    echo 'not ok 1 - intermediate failure prevented artifact copyback'
    exit 0
}
sh "$src_root/tools/ci-vm-result.sh" check "$result" >&2 && {
    echo 'not ok 1 - guest continued after failed build command'
    exit 0
}
sh "$src_root/tools/ci-vm-result.sh" capture "$result" <<'BUILD' || {
exit 0
BUILD
    echo 'not ok 1 - successful guest build was not captured'
    exit 0
}
sh "$src_root/tools/ci-vm-result.sh" check "$result" >&2 || {
    echo 'not ok 1 - host rejected successful guest build'
    exit 0
}
for invalid in '' invalid '0
1' '0
truncated'; do
    printf '%s' "$invalid" > "$result"
    sh "$src_root/tools/ci-vm-result.sh" check "$result" >&2 && {
        echo 'not ok 1 - invalid guest result accepted'
        exit 0
    }
done
sh "$src_root/tools/ci-vm-result.sh" check '' >&2 && {
    echo 'not ok 1 - missing guest result accepted'
    exit 0
}
echo 'ok 1 - VM build failures are validated after artifact copyback'
exit 0
