#!/bin/sh
# TAP test verifying remote paths bypass local existence checks.

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/path-remote-bypass.err"

run_sixel2png -i "https://example.invalid/test.six" -o/dev/null 2>"${err_file}" && {
    echo "not ok" 1 "remote input unexpectedly succeeded"
    exit 0
}

grep 'path "https://example.invalid/test.six" not found.' "${err_file}" >/dev/null 2>&1 && {
    echo "not ok" 1 "remote path was validated as a local filesystem path"
    exit 0
}

echo "ok" 1 "remote path bypassed local filesystem existence checks"
exit 0
