#!/bin/sh
# A single row supports more than two palette passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0041_single_row || {
    echo "not ok 1 - high-color single row"
    exit 0
}
echo "ok 1 - high-color single row"
exit 0
