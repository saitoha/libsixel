#!/bin/sh
# TAP test for issue #167 with height option before the input file.

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

check_exit -h128 "${issue167}" >"${ARTIFACT_LOCAL_DIR}/issue167-height-option-first.sixel" || {
    fail 1 "crafted height option failed"
    exit 0
}

pass 1 "crafted height option handled"

exit 0
