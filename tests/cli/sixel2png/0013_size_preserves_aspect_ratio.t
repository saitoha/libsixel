#!/bin/sh
# Verify sixel2png -s scales the longer edge and preserves aspect ratio.
# Policy: docs/functionality/decoding-pipeline.md
# Policy: docs/misc/platforms/solaris.md
# Coverage: SOL-03

set -eux

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "1..0 # SKIP sixel2png is disabled in this build"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

output_png="${ARTIFACT_LOCAL_DIR}/0013-size-aspect-ratio.png"

${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -s 31 \
    -i "${TOP_SRCDIR}/images/map8.six" -o "${output_png}" || {
    echo "not ok" 1 - "sixel2png size conversion failed"
    exit 0
}

# Hex bytes have a stable width across GNU and Solaris od implementations.
# shellcheck disable=SC2046
set -- $(od -An -tx1 -j16 -N8 "${output_png}")
test "$#" -eq 8 || {
    echo "not ok" 1 - "PNG IHDR dimensions are unavailable"
    exit 0
}
test "$1 $2 $3 $4 $5 $6 $7 $8" = "00 00 00 1f 00 00 00 05" || {
    echo "not ok" 1 - "93x14 input did not scale to 31x5"
    exit 0
}

echo "ok" 1 - "size preserves aspect ratio"
exit 0
