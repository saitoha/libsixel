#!/bin/sh
# Binary alpha keeps RGB888 despite bypassed clustering.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0072_packed_binary_byte || {
    printf '\n%s\n' "not ok 1 - high-color packed binary byte"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color packed binary byte"
exit 0
