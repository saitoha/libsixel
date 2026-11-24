#!/bin/sh
# Verify option atomicity.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0085_multi_pass_option_atomicity" || {
    printf '\n%s\n' "not ok 1 - option atomicity"
    exit 0
}

printf '\n%s\n' "ok 1 - option atomicity"
exit 0
