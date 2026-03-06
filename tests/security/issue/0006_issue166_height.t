#!/bin/sh
# TAP test for issue #166 crafted height input handling.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"
printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"


issue166="${TOP_SRCDIR}/tests/data/security/issue/data/166/poc"


set +e
run_img2sixel "${issue166}" -h128 >"${ARTIFACT_LOCAL_DIR}/issue166-height.sixel"
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    echo "not ok" 1 - "crafted height input failed"
    exit 0
}

echo "ok" 1 - "crafted height input handled"

exit 0
