#!/bin/sh
# Policy: docs/encoder/output/gamut-mapping.md
# Defensive coverage for the packed float gamut adapter.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" gamut/0060_float32_scalar_equivalence || {
    printf '\n%s\n' "not ok 1 - float32 scalar equivalence"
    exit 0
}
printf '\n%s\n' "ok 1 - float32 scalar equivalence"
exit 0
