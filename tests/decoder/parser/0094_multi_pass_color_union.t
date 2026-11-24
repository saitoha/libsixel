#!/bin/sh
# Verify color union.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0084_multi_pass_color_union" || {
    printf '\n%s\n' "not ok 1 - color union"
    exit 0
}

printf '\n%s\n' "ok 1 - color union"
exit 0
