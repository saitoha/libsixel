#!/bin/sh
# A short offset image retains its row bit over repeated passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0049_single_offset || {
    echo "not ok 1 - high-color single offset"
    exit 0
}
echo "ok 1 - high-color single offset"
exit 0
