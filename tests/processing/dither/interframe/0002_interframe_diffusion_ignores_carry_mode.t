#!/bin/sh
# TAP test confirming interframe ignores the legacy carry selector.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_direct="${ARTIFACT_LOCAL_DIR}/direct.six"
output_carry="${ARTIFACT_LOCAL_DIR}/carry.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d interframe -y raster -Y direct -p 16 \
    -o "${output_direct}" "${input_image}" || {
    echo "not ok" 1 - "interframe with -Y direct failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -d interframe -y raster -Y carry -p 16 \
    -o "${output_carry}" "${input_image}" || {
    echo "not ok" 1 - "interframe with -Y carry failed"
    exit 0
}

cmp -s "${output_direct}" "${output_carry}" || {
    echo "not ok" 1 - "interframe output changed by -Y"
    exit 0
}

echo "ok" 1 - "interframe ignores -Y carry mode"
exit 0
