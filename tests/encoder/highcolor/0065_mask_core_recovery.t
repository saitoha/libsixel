#!/bin/sh
# Mask validation, conversion failures and reuse restore the caller view.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0065_mask_core_recovery || {
    printf '\n%s\n' "not ok 1 - high-color mask core recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color mask core recovery"
exit 0
