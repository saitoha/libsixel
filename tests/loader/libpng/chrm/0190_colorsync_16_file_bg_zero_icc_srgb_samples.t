#!/bin/sh
# Test-plan: docs/testing/png-chrm-boundaries.md
# Verify colorsync 16-file-bg-zero-icc-srgb original samples and coverage.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader SIXEL_LOADER_CMS_TARGET_COLORSPACE=linear \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0198_png_chrm_samples 71 colorsync 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP requested libpng or CMS engine is not compiled"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - colorsync 16-file-bg-zero-icc-srgb original samples and coverage"
    exit 0
}
case "${trace}" in *"PNG_CHRM_SAMPLE|fixture=71|engine=colorsync|checked=24|"*) true ;; *) false ;; esac || {
    echo "not ok 1 - numeric observation did not complete"
    exit 0
}
# Consume every apply in order; partial or extra records must fail.
case "${trace}" in *"LSXCMS2|event=apply"*) true ;; *) false ;; esac || {
    echo "not ok 1 - ICC apply 1 is missing"
    exit 0
}
trace=${trace#*"LSXCMS2|event=apply"}
case "${trace}" in '|engine=colorsync|path=external|src=4|dst=4|ok=1
'*) true ;; *) false ;; esac || {
    echo "not ok 1 - ICC apply 1 fields differ"
    exit 0
}
trace=${trace#'|engine=colorsync|path=external|src=4|dst=4|ok=1
'}
case "${trace}" in *"LSXCMS2|event=apply"*) true ;; *) false ;; esac || {
    echo "not ok 1 - ICC apply 2 is missing"
    exit 0
}
trace=${trace#*"LSXCMS2|event=apply"}
case "${trace}" in '|engine=colorsync|path=external|src=4|dst=4|ok=1
'*) true ;; *) false ;; esac || {
    echo "not ok 1 - ICC apply 2 fields differ"
    exit 0
}
trace=${trace#'|engine=colorsync|path=external|src=4|dst=4|ok=1
'}
case "${trace}" in *"LSXCMS2|event=apply"*) false ;; *) true ;; esac || {
    echo "not ok 1 - unexpected ICC apply remains"
    exit 0
}
echo "ok 1 - colorsync 16-file-bg-zero-icc-srgb original samples and coverage"
exit 0
