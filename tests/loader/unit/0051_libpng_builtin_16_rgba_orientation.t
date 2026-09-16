#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# Verify libpng builtin 16-bit rgba original samples after EXIF 6.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0185_png_source_numeric libpng builtin 16 rgba 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-oriented-16-rgba.png" orientation 0 \
    2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested loader or CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - libpng builtin 16-bit rgba original samples after EXIF 6"
    exit 0
}
test "${trace#*"LSXLOAD1|event=ok|loader=libpng|code=L_OK"}" != "$trace" || {
    echo "not ok 1 - requested PNG loader did not deliver the frame"
    exit 0
}
test "${trace#*"LSXCMS2|event=apply|engine=builtin|path=extended-rgb|src=4|dst=4|ok=1"}" != "$trace" || {
    echo "not ok 1 - requested CMS did not apply to original float samples"
    exit 0
}
echo "ok 1 - libpng builtin 16-bit rgba original samples after EXIF 6"
exit 0
