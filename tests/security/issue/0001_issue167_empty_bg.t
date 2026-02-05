#!/bin/sh
# TAP test for issue #167 empty background argument handling.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"

tmp_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=${test_dir}
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"



check_exit() {
    if run_img2sixel "$@"; then
        rc=0
    else
        rc=$?
    fi

    # Accept success or mapped error exits (1/2/3) without crashing.
    case ${rc} in
        0|1|2|3)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

issue167="${top_srcdir}/tests/security/issue/data/167/poc"

printf '1..1\n'
set -v

if check_exit -B '#000' -B '' >"${output_dir}/issue167-empty-bg.sixel"; then
    pass 1 "empty background tolerated"
else
    fail 1 "empty background rejected"
fi

exit "${status}"
