#!/bin/sh
# Verify horizontal playback config.
# Policy: docs/encoder/output/delta-encoding.md
set -eux
echo "1..1"
set -v
status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    delta/refresh/0113_horizontal_playback_config || status=$?
test "$status" -eq 0 || {
    printf '\n%s\n' "not ok 1 - horizontal playback config"
    exit 0
}
printf '\n%s\n' "ok 1 - horizontal playback config"
exit 0
