#!/bin/sh
# TAP test: img2sixel fetches local files via the file scheme.

set -eux

test_name=$(basename "$0")
test_dir=$(CDPATH=; cd "$(dirname "$0")" && pwd)
category_name=$(basename "$(dirname "${test_dir}")")
artifact_root=${ARTIFACT_ROOT:-"$(pwd)/_artifacts"}
artifact_dir="${artifact_root}/${category_name}/${test_name}"
log_file="${artifact_dir}/curl.log"
output_dir="${artifact_dir}/outputs"

mkdir -p "${output_dir}"

script_dir=${test_dir}
. "${script_dir}/../../_lib/sh/common.sh"

if ! feature_defined_in_config "HAVE_LIBCURL"; then
    skip_all "libcurl is required for file scheme access"
fi

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

local_file="file://$(CDPATH=; cd "${top_srcdir}" && pwd)/images/snake.jpg"
if run_img2sixel "${local_file}" \
        >"${output_dir}/local-file.sixel" 2>>"${log_file}"; then
    printf 'ok 1 - fetches local file via file scheme\n'
else
    printf 'not ok 1 - local file fetch via file scheme failed\n'
    exit 1
fi
