#!/bin/sh
# TAP test: img2sixel fetches local files via the file scheme.

set -euxv

test_name=$(basename "$0")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${test_name}"
log_file="${artifact_dir}/curl.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
. "${script_dir}/../../common/t/0001_converters_common.t"

ensure_network_backend_available
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"

local_file="file://$(CDPATH=; cd "${top_srcdir}" && pwd)/images/snake.jpg"
if run_img2sixel "${local_file}" \
        >"${output_dir}/local-file.sixel" 2>>"${log_file}"; then
    printf 'ok 1 - fetches local file via file scheme\n'
else
    printf 'not ok 1 - local file fetch via file scheme failed\n'
    exit 1
fi
