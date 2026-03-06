#!/bin/sh
# Inspect Sixel metadata successfully.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_six="${TOP_SRCDIR}/images/map8.six"
target_txt="${ARTIFACT_LOCAL_DIR}/sixel-inspection.txt"

run_img2sixel -I "${snake_six}" >"${target_txt}" || {
    echo "not ok" 1 - "Sixel metadata inspection fails"
    exit 0
}

echo "ok" 1 - "Sixel metadata inspection succeeds"

exit 0
