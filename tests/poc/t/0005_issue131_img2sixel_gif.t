#!/bin/sh
# TAP test ensuring issue #131 PoC GIF is rejected without crashing.

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



issue131="${top_srcdir}/tests/issue/131/2020-01-30-img2sixel.gif"
require_file "${issue131}"

printf '1..1\n'
set -v

if run_img2sixel --high-color "${issue131}" \
        >"${output_dir}/issue131-high-color.sixel" \
        2>>"${log_file}"; then
    rc=0
else
    rc=$?
fi

# Expected behavior:
# - The PoC must be rejected (non-zero status).
# - It must not crash (exit 139 indicates SIGSEGV).
case ${rc} in
    0)
        fail 1 "issue #131 PoC unexpectedly accepted"
        ;;
    127)
        fail 1 "img2sixel was not executed as expected"
        ;;
    139)
        fail 1 "issue #131 PoC triggered SIGSEGV"
        ;;
    *)
        pass 1 "issue #131 PoC rejected without crashing"
        ;;
esac

exit "${status}"
