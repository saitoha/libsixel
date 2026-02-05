#!/bin/sh
# TAP test for issue #207 resize handling on crafted input.

set -eux

output_dir="${ARTIFACT_LOCAL_DIR}"

tmp_dir="${ARTIFACT_LOCAL_DIR}"


script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
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

issue207="${top_srcdir}/tests/security/issue/data/207/poc"
require_file "${issue207}"

printf '1..1\n'
set -v

if check_exit -h 50% -r lanczos3 -w 300px "${issue207}" \
        >"${output_dir}/issue207-resize.sixel"; then
    pass 1 "resize path handled"
else
    fail 1 "resize path failed"
fi

exit "${status}"
