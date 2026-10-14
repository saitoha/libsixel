#!/bin/sh
# Observe one animation cycle, including a missing-frame rejection.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/small-gif-first-samples.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/small.gif" 'builtin:Enone!' 0 99 || status=$?
test "${status}" -eq 1 || {
    echo "not ok 1 - animation missing frame"
    exit 0
}
echo "ok 1 - animation missing frame"
exit 0
