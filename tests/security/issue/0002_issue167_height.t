#!/bin/sh
# TAP test for issue #167 crafted height input handling.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"

tmp_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=$(CDPATH=; cd "${0%[/\\]*}" && pwd)
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
require_file "${issue167}"

printf '1..1\n'
set -v

if check_exit "${issue167}" -h128 >"${output_dir}/issue167-height.sixel"; then
    pass 1 "crafted height input handled"
else
    fail 1 "crafted height input failed"
fi

exit "${status}"
