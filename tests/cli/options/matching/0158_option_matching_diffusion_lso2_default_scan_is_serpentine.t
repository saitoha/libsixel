#!/bin/sh
# TAP test verifying -d lso2 defaults to serpentine scan.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.png"
output_default="${ARTIFACT_LOCAL_DIR}/output_default.six"
output_serp="${ARTIFACT_LOCAL_DIR}/output_serp.six"
output_raster="${ARTIFACT_LOCAL_DIR}/output_raster.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 \
    -d lso2 \
    -o "${output_default}" "${input_image}" || {
    echo "not ok" 1 - "lso2 default scan conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 \
    -d lso2:scan=serpentine \
    -o "${output_serp}" "${input_image}" || {
    echo "not ok" 1 - "lso2 serpentine conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 \
    -d lso2:scan=raster \
    -o "${output_raster}" "${input_image}" || {
    echo "not ok" 1 - "lso2 raster conversion failed"
    exit 0
}

cmp -s "${output_default}" "${output_serp}" || {
    echo "not ok" 1 - "lso2 default scan did not match serpentine"
    exit 0
}

cmp -s "${output_default}" "${output_raster}" && {
    echo "not ok" 1 - "lso2 default scan unexpectedly matched raster"
    exit 0
}

echo "ok" 1 - "lso2 default scan is serpentine"
exit 0
