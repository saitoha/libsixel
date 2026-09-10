#!/bin/sh
# Verify img2sixel PNG output snapshots the generated SIXEL stream.
# Policy: docs/writers/png.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_SIXEL2PNG-}" = 1 || {
    printf "1..0 # SKIP sixel2png is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

source_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.jpg"
sixel_path="${ARTIFACT_LOCAL_DIR}/dithered.six"
decoded_png="${ARTIFACT_LOCAL_DIR}/decoded.png"
snapshot_png="${ARTIFACT_LOCAL_DIR}/snapshot.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs -o "${sixel_path}" \
        "${source_image}" || {
    echo "not ok" 1 - "dithered SIXEL generation failed"
    exit 0
}

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -i "${sixel_path}" \
        -o "${decoded_png}" || {
    echo "not ok" 1 - "generated SIXEL decode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -d fs -o "png:${snapshot_png}" \
        "${source_image}" || {
    echo "not ok" 1 - "post-SIXEL PNG snapshot failed"
    exit 0
}

cmp -s "${decoded_png}" "${snapshot_png}" || {
    echo "not ok" 1 - "PNG snapshot differs from generated SIXEL decode"
    exit 0
}

echo "ok" 1 - "PNG snapshot matches generated dithered SIXEL decode"
exit 0
