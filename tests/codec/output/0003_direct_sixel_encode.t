#!/bin/sh
# Encode existing Sixel data directly.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || skip_all "img2sixel is disabled in this build"

echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/images/map8.six"
target_sixel="${ARTIFACT_LOCAL_DIR}/sixel-direct.sixel"

run_img2sixel -e "${snake_six}" >"${target_sixel}" || {
    fail 1 "direct Sixel encode failed"
    exit 0
}

pass 1 "direct Sixel encode emits data"
exit 0
