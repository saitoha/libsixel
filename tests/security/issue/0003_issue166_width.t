#!/bin/sh
# TAP test for issue #166 crafted width input handling.

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

issue166="${top_srcdir}/tests/security/issue/data/166/poc"

printf '1..1\n'
set -v

if check_exit "${issue166}" -w128 >"${output_dir}/issue166-width.sixel"; then
    pass 1 "crafted width input handled"
else
    fail 1 "crafted width input failed"
fi

exit "${status}"
