#!/bin/sh
# TAP test: img2sixel rejects malformed HTTPS URL without output.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_test_dir=$(dirname "$0")
artifact_dir="${artifact_root}/${artifact_test_dir}/${test_name}"
log_file="${artifact_dir}/curl.log"
tmp_dir="${artifact_dir}/tmp"

mkdir -p "${artifact_dir}" "${tmp_dir}"

script_dir=${test_dir}
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_network_backend_available
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

capture_file=$(make_temp_file "${tmp_dir}" "curl-capture")
run_img2sixel 'https:///test' >"${capture_file}" 2>>"${log_file}" || true

if [ -s "${capture_file}" ]; then
    printf 'not ok 1 - malformed HTTPS URL produced output\n'
    rm -f "${capture_file}"
    exit 1
fi

rm -f "${capture_file}"
printf 'ok 1 - rejects malformed HTTPS URL\n'
