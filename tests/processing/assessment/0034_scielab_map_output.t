#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0034_scielab_map_output" || {
    echo "not ok 1 - scielab_map_output"
    exit 0
}
echo "ok 1 - scielab_map_output"
exit 0
