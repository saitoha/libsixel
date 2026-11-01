#!/bin/sh
# PNG snapshot frame output reuse.
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0004_frame_output_reuse || {
    echo "not ok 1 - PNG snapshot frame output reuse"
    exit 0
}
echo "ok 1 - PNG snapshot frame output reuse"
exit 0
