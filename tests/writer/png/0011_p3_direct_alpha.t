#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "ok 1 # SKIP sixel2png disabled"
    exit 0
}
SIXEL_DECODER_INPUT_COLORSPACE=srgb ${SIXEL_RUNTIME-} \
    "${SIXEL2PNG_PATH}" -D -T display-p3 \
    <"${TOP_SRCDIR}/tests/writer/png/colors.six" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0008_p3_direct_alpha" || {
    echo "not ok 1 - p3_direct_alpha"
    exit 0
}
echo "ok 1 - p3_direct_alpha"
exit 0
