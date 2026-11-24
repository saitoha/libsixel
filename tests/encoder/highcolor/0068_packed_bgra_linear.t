#!/bin/sh
# BGRA P3 output retains values through linear work.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0068_packed_bgra_linear || {
    printf '\n%s\n' "not ok 1 - high-color packed bgra linear"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color packed bgra linear"
exit 0
