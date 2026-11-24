#!/bin/sh
# Repeated keys retain paint-time colors across pending passes.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0042_key_revisit || {
    printf '\n%s\n' "not ok 1 - high-color key revisit"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color key revisit"
exit 0
