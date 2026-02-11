#!/bin/sh
# TAP test for issue #207 resize handling on crafted input.

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

issue207="${top_srcdir}/tests/security/issue/data/207/poc"

printf '1..1\n'
set -v

check_exit -h 50% -r lanczos3 -w 300px "${issue207}"         >"${ARTIFACT_LOCAL_DIR}/issue207-resize.sixel" || {
    fail 1 "resize path failed"
    exit 0
}

pass 1 "resize path handled"

exit 0
