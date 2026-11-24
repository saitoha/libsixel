#!/bin/sh
# P3 encoder reuse clears and replaces borrowed masks.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0064_mask_reuse || {
    printf '\n%s\n' "not ok 1 - high-color mask reuse"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color mask reuse"
exit 0
