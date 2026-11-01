#!/bin/sh
# Defensive coverage for the packed float gamut adapter.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0060_float32_scalar_equivalence || {
    echo "not ok 1 - float32 scalar equivalence"
    exit 0
}
echo "ok 1 - float32 scalar equivalence"
exit 0
