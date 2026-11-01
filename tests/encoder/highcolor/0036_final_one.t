#!/bin/sh
# One-row final band completes pending colors.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0036_final_one || {
    echo "not ok 1 - high-color final one"
    exit 0
}
echo "ok 1 - high-color final one"
exit 0
