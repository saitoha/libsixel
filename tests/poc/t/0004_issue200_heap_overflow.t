#!/bin/sh
# TAP test ensuring issue #200 heap overflow is avoided.

set -eux

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



issue200="${top_srcdir}/tests/issue/200/POC_img2sixel_heap_buffer_overflow"
require_file "${issue200}"

printf '1..1\n'
set -v

if run_img2sixel --7bit-mode -8 --invert --palette-type=auto --verbose \
        "${issue200}" -o /dev/null 2>>"${log_file}"; then
    pass 1 "heap overflow regression avoided"
else
    fail 1 "heap overflow regression triggered"
fi

exit "${status}"
