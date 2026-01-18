#!/bin/sh
# TAP test ensuring builtin loader handles HDR inputs without fallback.

set -euxv

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/builtin-hdr.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

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

check_sixel_output() {
    sixel_file=$1

    if [ ! -s "${sixel_file}" ]; then
        return 1
    fi

    sixel_hex=$(od -An -tx1 "${sixel_file}" | tr -d ' \n')
    case "${sixel_hex}" in
        1b5071*) ;;
        *) return 1 ;;
    esac
    case "${sixel_hex}" in
        *1b5c) ;;
        *) return 1 ;;
    esac

    if ! LC_ALL=C grep -Eq '#[0-9]+;2;100;0;0' "${sixel_file}"; then
        return 1
    fi

    return 0
}

echo "1..1"

hdr_input="${top_srcdir}/tests/data/inputs/formats/stbi_minimal.hdr"
target_sixel="${output_dir}/builtin-hdr.sixel"

require_file "${hdr_input}"

if run_img2sixel -Lbuiltin -v -w1 -h1 "${hdr_input}" \
        >"${target_sixel}" 2>"${log_file}"; then
    if ! grep -q "libsixel: loader builtin succeeded" "${log_file}"; then
        fail ${case_id} "builtin HDR did not report success"
    elif ! check_sixel_output "${target_sixel}"; then
        fail ${case_id} "builtin HDR output content mismatch"
    else
        pass ${case_id} "builtin HDR conversion succeeds"
    fi
else
    fail ${case_id} "builtin HDR conversion failed"
fi

exit "${status}"
