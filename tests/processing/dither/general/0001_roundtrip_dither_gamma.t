#!/bin/sh
# Verify round-trip conversion with dithering and gamma correction.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

snake_jpg="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
snake_stage="${ARTIFACT_LOCAL_DIR}/snake-stage.sixel"
snake_roundtrip="${ARTIFACT_LOCAL_DIR}/snake-roundtrip.sixel"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "${snake_jpg}" -datkinson -flum -save \
    >"${snake_stage}" || {
    echo "not ok" 1 - "round-trip conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" <"${snake_stage}" >"${snake_roundtrip}" || {
    echo "not ok" 1 - "round-trip conversion failed"
    exit 0
}

echo "ok" 1 - "round-trip conversion with dithering and gamma succeeded"

exit 0
