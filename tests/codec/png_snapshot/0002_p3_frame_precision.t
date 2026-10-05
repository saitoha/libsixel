#!/bin/sh
# PNG snapshot p3 frame precision.
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0002_p3_frame_precision || {
    echo "not ok 1 - PNG snapshot p3 frame precision"
    exit 0
}
echo "ok 1 - PNG snapshot p3 frame precision"
exit 0
