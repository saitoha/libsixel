#!/bin/sh
# Verify mixed framing.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0059_multi_pass_mixed_framing" || {
    printf '\n%s\n' "not ok 1 - mixed framing"
    exit 0
}

printf '\n%s\n' "ok 1 - mixed framing"
exit 0
