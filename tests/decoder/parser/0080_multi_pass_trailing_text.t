#!/bin/sh
# Verify trailing text.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0070_multi_pass_trailing_text" || {
    printf '\n%s\n' "not ok 1 - trailing text"
    exit 0
}

printf '\n%s\n' "ok 1 - trailing text"
exit 0
