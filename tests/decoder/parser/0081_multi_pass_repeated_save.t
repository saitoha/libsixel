#!/bin/sh
# Verify repeated save.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0071_multi_pass_repeated_save" || {
    printf '\n%s\n' "not ok 1 - repeated save"
    exit 0
}

printf '\n%s\n' "ok 1 - repeated save"
exit 0
