#!/bin/sh
# TAP test verifying img2sixel rejects non-image stdin data without output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

output_file="${ARTIFACT_LOCAL_DIR}/capture.stdin"

echo a | ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" - >"${output_file}" && :

test ! -s "${output_file}" || {
    echo "not ok" 1 - "img2sixel produced output for invalid stdin"
    exit 0
}

echo "ok" 1 - "invalid stdin rejected without output"
exit 0
