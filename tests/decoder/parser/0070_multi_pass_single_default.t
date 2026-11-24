#!/bin/sh
# Verify single default.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0061_multi_pass_single_default" || {
    printf '\n%s\n' "not ok 1 - single default"
    exit 0
}

printf '\n%s\n' "ok 1 - single default"
exit 0
