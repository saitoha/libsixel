#!/bin/sh
# Exercise the float32 FHEDT lookup policy through img2sixel options.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_png="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/fhedt-float32.six"

run_img2sixel --lookup-policy=fhedt --precision=float32 -p 16 -d none \
        -o "${output_sixel}" "${snake_png}" || {
    echo "not ok" 1 - "float32 FHEDT lookup policy failed"
    exit 0
}

echo "ok" 1 - "float32 FHEDT lookup policy completes"

exit 0
