#!/bin/sh
# Ordinary medoids retains extended source representatives in five spaces.
# Policy: docs/encoder/palette-construction/quantization/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/ordinary_float/medoids_sources" || {
    printf '\n%s\n' "not ok 1 - Ordinary medoids retains extended source representatives in five spaces"
    exit 0
}
printf '\n%s\n' "ok 1 - Ordinary medoids retains extended source representatives in five spaces"
exit 0
