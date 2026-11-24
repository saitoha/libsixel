#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Verify the sampling fallback resolution contract.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0029_sampling_fallback_resolution" || {
    printf '\n%s\n' "not ok 1 - sampling fallback resolution"
    exit 0
}

printf '\n%s\n' "ok 1 - sampling fallback resolution"
exit 0
