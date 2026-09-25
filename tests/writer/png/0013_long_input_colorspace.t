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
    "${SIXEL2PNG_PATH}" --input-colorspace=display-p3 \
    <"${TOP_SRCDIR}/tests/writer/png/colors.six" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0013_long_input_colorspace" || {
    echo "not ok 1 - long_input_colorspace"
    exit 0
}
echo "ok 1 - long_input_colorspace"
exit 0
