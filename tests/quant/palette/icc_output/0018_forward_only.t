#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_THREADS=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output parse 2 "icc:P${TOP_SRCDIR}/tests/data/color-numeric/forward-lut.icc" || {
    echo "not ok 1 - ICC forward only"
    exit 0
}
echo "ok 1 - ICC forward only"
exit 0
