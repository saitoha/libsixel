#!/bin/sh
# TAP test verifying non-lso2 diffusion defaults to raster scan.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_default="${ARTIFACT_LOCAL_DIR}/output_default.six"
output_raster="${ARTIFACT_LOCAL_DIR}/output_raster.six"
output_serp="${ARTIFACT_LOCAL_DIR}/output_serp.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 \
    -d fs \
    -o "${output_default}" "${input_image}" || {
    echo "not ok" 1 - "fs default scan conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 \
    -d fs:scan=raster \
    -o "${output_raster}" "${input_image}" || {
    echo "not ok" 1 - "fs raster conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 \
    -d fs:scan=serpentine \
    -o "${output_serp}" "${input_image}" || {
    echo "not ok" 1 - "fs serpentine conversion failed"
    exit 0
}

cmp -s "${output_default}" "${output_raster}" || {
    echo "not ok" 1 - "fs default scan did not match raster"
    exit 0
}

cmp -s "${output_default}" "${output_serp}" && {
    echo "not ok" 1 - "fs default scan unexpectedly matched serpentine"
    exit 0
}

echo "ok" 1 - "non-lso2 default scan is raster"
exit 0
