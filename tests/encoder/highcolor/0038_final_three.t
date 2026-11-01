#!/bin/sh
# Three-row final band completes pending colors.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0038_final_three || {
    echo "not ok 1 - high-color final three"
    exit 0
}
echo "ok 1 - high-color final three"
exit 0
