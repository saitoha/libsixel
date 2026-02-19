#!/bin/sh
# TAP test for issue #207 resize handling on crafted input.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


issue207="${TOP_SRCDIR}/tests/data/security/issue/data/207/poc"

printf '1..1\n'
set -v

set +e
run_img2sixel -h 50% -r lanczos3 -w 300px "${issue207}"         >"${ARTIFACT_LOCAL_DIR}/issue207-resize.sixel"
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    fail 1 "resize path failed"
    exit 0
}

pass 1 "resize path handled"

exit 0
