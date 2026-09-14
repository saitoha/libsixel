#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify blue inside boundary has exact XYZ.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0196_png_chrm_boundary 3 1 2>&1) || result=$?
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - blue inside boundary has exact XYZ"
    exit 0
}
test "${trace#*"PNG_CHRM_GRID|boundary|point=3|inside=1|checked=3"}" != "$trace" || {
    echo "not ok 1 - numeric observation did not complete"
    exit 0
}
echo "ok 1 - blue inside boundary has exact XYZ"
exit 0
