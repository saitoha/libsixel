#!/bin/sh
# Inspect Sixel metadata successfully.
set -eux

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

echo "1..1"
set -v

snake_six="${TOP_SRCDIR}/images/map8.six"
target_txt="${ARTIFACT_LOCAL_DIR}/sixel-inspection.txt"

run_img2sixel -I "${snake_six}" >"${target_txt}" || {
    fail 1 "Sixel metadata inspection fails"
    exit 0
}

pass 1 "Sixel metadata inspection succeeds"

exit 0
