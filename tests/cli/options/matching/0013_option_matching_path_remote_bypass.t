#!/bin/sh
# TAP test verifying remote paths bypass local existence checks.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build";
    exit 0
}

echo "1..1"
set -v

err_file="${ARTIFACT_LOCAL_DIR}/path-remote-bypass.err"

run_sixel2png -i "https://example.invalid/test.six" -o/dev/null 2>"${err_file}" && {
    fail 1 "remote input unexpectedly succeeded"
    exit 0
}

grep 'path "https://example.invalid/test.six" not found.' "${err_file}" >/dev/null 2>&1 && {
    fail 1 "remote path was validated as a local filesystem path"
    exit 0
}

pass 1 "remote path bypassed local filesystem existence checks"
exit 0
