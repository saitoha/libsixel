#!/bin/sh
# Policy: docs/cli/diagnostics.md
# Pin the sample-extraction interval of the encoder timeline.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "timeline/0007_timeline_sample_interval" || {
    printf '\n%s\n' "not ok 1 - sample interval contract changed"
    exit 0
}

printf '\n%s\n' "ok 1 - sample extraction records one interval before palette work"
exit 0
