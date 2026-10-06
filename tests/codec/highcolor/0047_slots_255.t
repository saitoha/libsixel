#!/bin/sh
# Exactly 255 keys finish without a pending pass.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0047_slots_255 || {
    echo "not ok 1 - high-color slots 255"
    exit 0
}
echo "ok 1 - high-color slots 255"
exit 0
