#!/bin/sh
# TAP test: img2sixel rejects invalid file URL without producing output.

set -eux

script_dir=${test_dir}
. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_network_backend_available
ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

capture_file=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "curl-capture")
run_img2sixel 'file:///test' >"${capture_file}" || true

if [ -s "${capture_file}" ]; then
    printf 'not ok 1 - invalid file URL produced output\n'
    rm -f "${capture_file}"
    exit 1
fi

rm -f "${capture_file}"
printf 'ok 1 - rejects invalid file URL\n'
