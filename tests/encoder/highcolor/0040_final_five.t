#!/bin/sh
# Five-row final band completes pending colors.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0040_final_five || {
    echo "not ok 1 - high-color final five"
    exit 0
}
echo "ok 1 - high-color final five"
exit 0
