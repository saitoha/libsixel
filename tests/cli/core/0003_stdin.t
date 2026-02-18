#!/bin/sh
# TAP test verifying img2sixel rejects non-image stdin data without output.

set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

output_file="${ARTIFACT_LOCAL_DIR}/capture.stdin"

echo a | run_img2sixel - >"${output_file}" && :

test ! -s "${output_file}" || {
    fail 1 "img2sixel produced output for invalid stdin"
    exit 0
}

pass 1 "invalid stdin rejected without output"
exit 0
