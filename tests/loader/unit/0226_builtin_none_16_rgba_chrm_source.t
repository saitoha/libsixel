#!/bin/sh
# Policy: docs/loader/builtin/png-source-precision.md
# Verify builtin none 16-bit rgba wide-gamut cHRM original samples.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0185_png_source_numeric builtin none 16 rgba 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-wide-chrm-16-rgba.png" chrm 0 \
    2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested loader or CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - builtin none 16-bit rgba wide-gamut cHRM original samples"
    exit 0
}
test "${trace#*"LSXLOAD1|event=ok|loader=builtin|code=L_OK"}" != "$trace" || {
    echo "not ok 1 - requested PNG loader did not deliver the frame"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply"}" = "$trace" || {
    echo "not ok 1 - ICC transform unexpectedly applied to cHRM-only input"
    exit 0
}
echo "ok 1 - builtin none 16-bit rgba wide-gamut cHRM original samples"
exit 0
