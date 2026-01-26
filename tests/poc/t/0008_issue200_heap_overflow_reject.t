#!/bin/sh
# TAP test for issue #200 regression using the reported CLI flags.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
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

issue200="${top_srcdir}/tests/issue/200/POC_img2sixel_heap_buffer_overflow"
require_file "${issue200}"

printf '1..1\n'

if check_exit --7bit-mode -8 --invert --palette-type=auto --verbose \
        "${issue200}" -o /dev/null; then
    pass 1 "reported heap overflow path rejected safely"
else
    fail 1 "reported heap overflow path failed"
fi

exit "${status}"
