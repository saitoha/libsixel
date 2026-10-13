#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_OUTPUT_COLORSPACE_ICC_PATH="${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" SIXEL_THREADS=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output encode 0 "icc" || {
    echo "not ok 1 - ICC output env"
    exit 0
}
echo "ok 1 - ICC output env"
exit 0
