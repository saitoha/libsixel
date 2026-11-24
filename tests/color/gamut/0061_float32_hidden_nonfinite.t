#!/bin/sh
# Defensive coverage for the packed float gamut adapter.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0061_float32_hidden_nonfinite || {
    printf '\n%s\n' "not ok 1 - float32 hidden nonfinite"
    exit 0
}
printf '\n%s\n' "ok 1 - float32 hidden nonfinite"
exit 0
