#!/bin/sh
# Verify terminal control.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0076_multi_pass_terminal_control" || {
    printf '\n%s\n' "not ok 1 - terminal control"
    exit 0
}

printf '\n%s\n' "ok 1 - terminal control"
exit 0
