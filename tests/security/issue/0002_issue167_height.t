#!/bin/sh
# TAP test for issue #167 crafted height input handling.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



check_exit() {
    set +e
    run_img2sixel "$@"
    rc=$?
    set -e

    # Accept success or mapped error exits (1/2/3) without crashing.
    [ "${rc}" -le 3 ]
}

issue167="${top_srcdir}/tests/security/issue/data/167/poc"

printf '1..1\n'
set -v

check_exit "${issue167}" -h128 >"${ARTIFACT_LOCAL_DIR}/issue167-height.sixel" || {
    fail 1 "crafted height input failed"
    exit 0
}

pass 1 "crafted height input handled"

exit 0
