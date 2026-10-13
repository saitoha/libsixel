#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_THREADS=1 ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/icc_output/0001_icc_output encode 7 \
    "icc:path=${TOP_SRCDIR}/tests/data/color-numeric/display-p3.icc" || {
    echo "not ok 1 - ICC profile_reuse"
    exit 0
}
echo "ok 1 - ICC profile_reuse"
exit 0
