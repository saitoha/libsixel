#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0035_scielab_map_capacity" || {
    printf '\n%s\n' "not ok 1 - scielab_map_capacity"
    exit 0
}
printf '\n%s\n' "ok 1 - scielab_map_capacity"
exit 0
