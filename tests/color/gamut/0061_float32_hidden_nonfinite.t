#!/bin/sh
# Defensive coverage for the packed float gamut adapter.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0061_float32_hidden_nonfinite || {
    echo "not ok 1 - float32 hidden nonfinite"
    exit 0
}
echo "ok 1 - float32 hidden nonfinite"
exit 0
