#!/bin/sh
# P3 RGB preserves visible colors and separated omissions.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0052_mask_p3 || {
    echo "not ok 1 - high-color mask p3"
    exit 0
}
echo "ok 1 - high-color mask p3"
exit 0
