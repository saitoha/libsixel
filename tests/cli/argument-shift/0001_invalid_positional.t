#!/bin/sh
# TAP test verifying img2sixel rejects invalid positional inputs without
# emitting stray output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

ensure_converter_available "IMG2SIXEL" "${IMG2SIXEL_PATH}" "img2sixel"

echo "1..1"
set -v

missing_path="${ARTIFACT_LOCAL_DIR}/invalid_filename"
missing_output=$(make_temp_file "${ARTIFACT_LOCAL_DIR}" "capture.invalid")

run_img2sixel -v "${missing_path}" >"${missing_output}" && {
    fail 1 "img2sixel accepted missing input"
    exit 0
}

test ! -s "${missing_output}" || {
    fail 1 "img2sixel produced output for missing input"
    exit 0
}

pass 1 "missing input rejected without output"
exit 0
