#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

test "${HAVE_SIXEL2PNG-}" = 1 || {
    echo "ok 1 # SKIP sixel2png disabled"
    exit 0
}
${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" -D \
    -T "icc:path=${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" \
    <"${TOP_SRCDIR}/tests/writer/png/colors.six" | \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output png 1 unused || {
    echo "not ok 1 - ICC png_direct samples and metadata"
    exit 0
}
echo "ok 1 - ICC png_direct samples and metadata"
exit 0
