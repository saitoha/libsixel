#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# Verify libpng none 16-bit original alpha with gamma background and composite.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0185_png_source_numeric libpng none 16 rgba 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-background-16-rgba.png" gamma 0 \
    2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested loader or CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - libpng none 16-bit original alpha with gamma background and composite"
    exit 0
}
test "${trace#*"LSXLOAD1|event=ok|loader=libpng|code=L_OK"}" != "$trace" || {
    echo "not ok 1 - requested PNG loader did not deliver the frame"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply"}" = "$trace" || {
    echo "not ok 1 - disabled CMS unexpectedly applied"
    exit 0
}
echo "ok 1 - libpng none 16-bit original alpha with gamma background and composite"
exit 0
