#!/bin/sh
# ABGR P3 output retains values through Oklab work.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0069_packed_abgr_oklab || {
    echo "not ok 1 - high-color packed abgr oklab"
    exit 0
}
echo "ok 1 - high-color packed abgr oklab"
exit 0
