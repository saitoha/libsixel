#!/bin/sh
# TAP test: img2sixel rejects invalid file URL without producing output.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/curl.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${artifact_dir}" "${tmp_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_network_backend_available
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"

capture_file=$(make_temp_file "${tmp_dir}" "curl-capture")
run_img2sixel 'file:///test' >"${capture_file}" 2>>"${log_file}" || true

if [ -s "${capture_file}" ]; then
    printf 'not ok 1 - invalid file URL produced output\n'
    rm -f "${capture_file}"
    exit 1
fi

rm -f "${capture_file}"
printf 'ok 1 - rejects invalid file URL\n'
