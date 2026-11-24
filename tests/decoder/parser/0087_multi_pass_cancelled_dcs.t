#!/bin/sh
# Verify cancelled dcs.
# Policy: docs/decoder/multi-pass.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0077_multi_pass_cancelled_dcs" || {
    printf '\n%s\n' "not ok 1 - cancelled dcs"
    exit 0
}

printf '\n%s\n' "ok 1 - cancelled dcs"
exit 0
