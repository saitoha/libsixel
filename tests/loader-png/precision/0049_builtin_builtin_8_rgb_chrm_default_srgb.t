#!/bin/sh
# Policy: docs/loader/builtin/png-source-precision.md
# Verify builtin builtin 8-bit rgb isolated cHRM default sRGB samples.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0185_png_source_numeric builtin builtin 8 rgb 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-wide-chrm-8-rgb.png" chrm-default 0 \
    2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested loader or CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - builtin builtin 8-bit rgb isolated cHRM default sRGB samples"
    exit 0
}
case "${trace}" in *"LSXLOAD1|event=ok|loader=builtin|code=L_OK"*) true ;; *) false ;; esac || {
    echo "not ok 1 - requested PNG loader did not deliver the frame"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply"*) false ;; *) true ;; esac || {
    echo "not ok 1 - ICC transform unexpectedly applied to cHRM-only input"
    exit 0
}
echo "ok 1 - builtin builtin 8-bit rgb isolated cHRM default sRGB samples"
exit 0
