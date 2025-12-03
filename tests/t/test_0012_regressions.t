#!/bin/sh
# TAP test covering img2sixel regressions tied to filed issues.

set -eu

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/regressions.log"
output_dir="${artifact_dir}/outputs"

tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/converters-common.t"

status=0
case_id=1

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

echo "1..4"

issue167="${top_srcdir}/tests/issue/167/poc"
issue166="${top_srcdir}/tests/issue/166/poc"
issue200="${top_srcdir}/tests/issue/200/POC_img2sixel_heap_buffer_overflow"

require_file "${issue167}"
require_file "${issue166}"
require_file "${issue200}"

check_exit() {
    if run_img2sixel "$@" 2>>"${log_file}"; then
        rc=0
    else
        rc=$?
    fi

    case ${rc} in
        0|127|255)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

if check_exit -B '#000' -B '' >"${output_dir}/issue167-empty-bg.sixel"; then
    pass ${case_id} "empty background tolerated"
else
    fail ${case_id} "empty background rejected"
fi
case_id=$((case_id + 1))

if check_exit "${issue167}" -h128 >"${output_dir}/issue167-height.sixel"; then
    pass ${case_id} "crafted height input handled"
else
    fail ${case_id} "crafted height input failed"
fi
case_id=$((case_id + 1))

if check_exit "${issue166}" -w128 >"${output_dir}/issue166-width.sixel"; then
    pass ${case_id} "crafted width input handled"
else
    fail ${case_id} "crafted width input failed"
fi
case_id=$((case_id + 1))

if run_img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose \
        "${issue200}" -o /dev/null 2>>"${log_file}"; then
    pass ${case_id} "heap overflow regression avoided"
else
    fail ${case_id} "heap overflow regression triggered"
fi

exit "${status}"
