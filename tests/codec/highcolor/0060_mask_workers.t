#!/bin/sh
# Parallel body serialization preserves source-mask holes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threads disabled"
    exit 0
}

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0060_mask_workers || {
    echo "not ok 1 - high-color mask workers"
    exit 0
}
echo "ok 1 - high-color mask workers"
exit 0
