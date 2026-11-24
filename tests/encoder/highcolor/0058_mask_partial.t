#!/bin/sh
# A source mask follows partial final-band rewinds.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0058_mask_partial || {
    printf '\n%s\n' "not ok 1 - high-color mask partial"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color mask partial"
exit 0
