#!/bin/sh
# Emit monochrome frame output.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
target_sixel="${ARTIFACT_LOCAL_DIR}/monochrome-frame.sixel"

run_img2sixel -p 1 -h100 -n1 "${snake_jpg}" >"${target_sixel}" || {
    fail 1 "monochrome frame conversion fails"
    exit 0
}

pass 1 "monochrome frame conversion succeeds"
exit 0
