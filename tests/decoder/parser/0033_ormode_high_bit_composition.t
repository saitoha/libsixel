#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Distinguish this OR composition rule from ordinary painting.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0031_ormode_high_bit_composition" || {
    printf '\n%s\n' "not ok 1 - ormode_high_bit_composition"
    exit 0
}

printf '\n%s\n' "ok 1 - ormode_high_bit_composition"
exit 0
