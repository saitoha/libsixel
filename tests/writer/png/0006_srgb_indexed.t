#!/bin/sh
# Policy: docs/writers/png.md
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "ok 1 # SKIP sixel2png disabled"
    exit 0
}
SIXEL_DECODER_INPUT_COLORSPACE=srgb ${SIXEL_RUNTIME-} \
    "${SIXEL2PNG_PATH}"  \
    <"${TOP_SRCDIR}/tests/writer/png/colors.six" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0006_srgb_indexed" || {
    echo "not ok 1 - srgb_indexed"
    exit 0
}
echo "ok 1 - srgb_indexed"
exit 0
