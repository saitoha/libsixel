#!/bin/sh
# TAP test for issue #167 crafted height input handling.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"


issue167="${top_srcdir}/tests/security/issue/data/167/poc"

printf '1..1\n'
set -v

set +e
run_img2sixel "${issue167}" -h128 >"${ARTIFACT_LOCAL_DIR}/issue167-height.sixel"
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    fail 1 "crafted height input failed"
    exit 0
}

pass 1 "crafted height input handled"

exit 0
