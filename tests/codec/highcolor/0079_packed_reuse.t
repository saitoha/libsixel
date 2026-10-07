#!/bin/sh
# One encoder switches P3 intent and alpha interpretation.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0079_packed_reuse || {
    echo "not ok 1 - high-color packed reuse"
    exit 0
}
echo "ok 1 - high-color packed reuse"
exit 0
