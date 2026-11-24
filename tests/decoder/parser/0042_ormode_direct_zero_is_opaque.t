#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Resolve an untouched OR cell through palette zero with alpha 255.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0040_ormode_direct_zero_is_opaque" || {
    printf '\n%s\n' "not ok 1 - ormode_direct_zero_is_opaque"
    exit 0
}

printf '\n%s\n' "ok 1 - ormode_direct_zero_is_opaque"
exit 0
