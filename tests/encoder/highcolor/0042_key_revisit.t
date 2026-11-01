#!/bin/sh
# Repeated keys retain paint-time colors across pending passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0042_key_revisit || {
    echo "not ok 1 - high-color key revisit"
    exit 0
}
echo "ok 1 - high-color key revisit"
exit 0
