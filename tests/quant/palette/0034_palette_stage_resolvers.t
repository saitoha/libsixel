#!/bin/sh
# Verify pure stage-boundary sampling and binning resolution.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/0034_palette_stage_resolvers" || {
    echo "not ok 1 - palette stage resolvers"
    exit 0
}

echo "ok 1 - palette stage resolvers"
exit 0
