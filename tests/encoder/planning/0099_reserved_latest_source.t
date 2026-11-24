#!/bin/sh
# Policy: docs/encoder/palette-construction/pipeline/README.md
# Latest palette publication retains fixed colors and samples only video.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "planner/pipeline/0099_reserved_latest_source" || {
    printf '\n%s\n' "not ok 1 - reserved latest source"
    exit 0
}
printf '\n%s\n' "ok 1 - reserved latest source"
exit 0
