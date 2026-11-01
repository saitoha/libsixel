#!/bin/sh
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "ok 1 # SKIP sixel2png disabled"
    exit 0
}
SIXEL_DECODER_INPUT_COLORSPACE=display-p3 ${SIXEL_RUNTIME-} \
    "${SIXEL2PNG_PATH}"  \
    <"${TOP_SRCDIR}/tests/writer/png/colors.six" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "writer/png/0010_p3_environment" || {
    echo "not ok 1 - p3_environment"
    exit 0
}
echo "ok 1 - p3_environment"
exit 0
