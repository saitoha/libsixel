#!/bin/sh
# PNG snapshot byte frame control.
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0003_byte_frame_control || {
    echo "not ok 1 - PNG snapshot byte frame control"
    exit 0
}
echo "ok 1 - PNG snapshot byte frame control"
exit 0
