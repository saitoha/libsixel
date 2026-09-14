#!/bin/sh
# Policy: docs/loader/builtin/png-source-precision.md
# Verify builtin colorsync 8-bit rgba source samples, prefer=1.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0185_png_source_numeric builtin colorsync 8 rgba 1 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-8-rgba.png" \
    2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested loader or CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - builtin colorsync 8-bit rgba source samples, prefer=1"
    exit 0
}
test "${trace#*"LSXLOAD1|event=ok|loader=builtin|code=L_OK"}" != "$trace" || {
    echo "not ok 1 - requested PNG loader did not deliver the frame"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=colorsync|path=external|src=4|dst=4|ok=1"}" != "$trace" || {
    echo "not ok 1 - requested CMS did not apply to original float samples"
    exit 0
}
echo "ok 1 - builtin colorsync 8-bit rgba source samples, prefer=1"
exit 0
