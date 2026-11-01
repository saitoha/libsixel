#!/bin/sh
# A source mask follows repeated palette passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0056_mask_multipass || {
    echo "not ok 1 - high-color mask multipass"
    exit 0
}
echo "ok 1 - high-color mask multipass"
exit 0
