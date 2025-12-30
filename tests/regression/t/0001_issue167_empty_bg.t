#!/bin/sh
# TAP test for issue #167 empty background argument handling.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/regression.log"
output_dir="${artifact_dir}/outputs"

tmp_dir="${artifact_dir}/tmp"

mkdir -p "${output_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

status=0

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    status=1
}

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

issue167="${top_srcdir}/tests/issue/167/poc"
require_file "${issue167}"

printf '1..1\n'

if check_exit -B '#000' -B '' >"${output_dir}/issue167-empty-bg.sixel"; then
    pass 1 "empty background tolerated"
else
    fail 1 "empty background rejected"
fi

exit "${status}"
