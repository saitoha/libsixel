#!/bin/sh
# TAP test for issue #167 with height option before the input file.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build";
    exit 0
}


issue167="${TOP_SRCDIR}/tests/security/issue/data/167/poc"

printf '1..1\n'
set -v

set +e
run_img2sixel -h128 "${issue167}" >"${ARTIFACT_LOCAL_DIR}/issue167-height-option-first.sixel"
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    fail 1 "crafted height option failed"
    exit 0
}

pass 1 "crafted height option handled"

exit 0
