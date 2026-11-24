#!/bin/sh
# Verify default palette direct.
# Policy: docs/decoder/pipeline/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "decoder/0042_default_palette_direct" || {
    printf '\n%s\n' "not ok 1 - 0042_default_palette_direct"
    exit 0
}

printf '\n%s\n' "ok 1 - 0042_default_palette_direct"
exit 0
