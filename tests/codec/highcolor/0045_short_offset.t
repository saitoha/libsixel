#!/bin/sh
# A multi-band top margin is emitted only once across short passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0045_short_offset || {
    echo "not ok 1 - high-color short offset"
    exit 0
}
echo "ok 1 - high-color short offset"
exit 0
