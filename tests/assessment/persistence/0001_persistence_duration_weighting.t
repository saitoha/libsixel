#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/0036_persistence_duration_weighting || {
    printf '\n%s\n' "not ok 1 - persistence duration weighting"
    exit 0
}
printf '\n%s\n' "ok 1 - persistence duration weighting"
exit 0
