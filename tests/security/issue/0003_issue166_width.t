#!/bin/sh
# TAP test for issue #166 crafted width input handling.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"


issue166="${top_srcdir}/tests/security/issue/data/166/poc"

printf '1..1\n'
set -v

set +e
run_img2sixel "${issue166}" -w128 >"${ARTIFACT_LOCAL_DIR}/issue166-width.sixel"
command_status=$?
set -e

# Accept success or mapped error exits (1/2/3) without crashing.
test "${command_status}" -le 3 || {
    fail 1 "crafted width input failed"
    exit 0
}

pass 1 "crafted width input handled"

exit 0
