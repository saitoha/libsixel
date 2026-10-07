#!/bin/sh
# P3 masks and representatives survive pending final passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0057_mask_p3_multipass || {
    echo "not ok 1 - high-color mask p3 multipass"
    exit 0
}
echo "ok 1 - high-color mask p3 multipass"
exit 0
