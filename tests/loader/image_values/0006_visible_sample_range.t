#!/bin/sh
# Verify the RGB range observer accepts distinct component values.
set -eux

echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_sample_range \
    "${TOP_SRCDIR}/tests/data/color-numeric/red.six" 1 || status=$?
test "${status}" -eq 0 || {
    echo "not ok 1 - sample range control returned an unexpected status"
    exit 0
}
echo "ok 1 - RGB sample range control"
exit 0
