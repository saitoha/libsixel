#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify colorsync 16-file-bg-collinear-0-priority-bad-icc original samples and coverage.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0198_png_chrm_samples 208 colorsync 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested libpng or CMS engine is not compiled"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - colorsync 16-file-bg-collinear-0-priority-bad-icc original samples and coverage"
    exit 0
}
test "${trace#*"PNG_CHRM_SAMPLE|fixture=208|engine=colorsync|checked=24|"}" != "$trace" || {
    echo "not ok 1 - numeric observation did not complete"
    exit 0
}
# Consume every apply in order; partial or extra records must fail.
test "${trace#*"LSXCMS2|event=apply"}" = "$trace" || {
    echo "not ok 1 - unexpected ICC apply remains"
    exit 0
}
echo "ok 1 - colorsync 16-file-bg-collinear-0-priority-bad-icc original samples and coverage"
exit 0
