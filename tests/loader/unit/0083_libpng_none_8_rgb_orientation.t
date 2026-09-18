#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# Verify libpng none 8-bit rgb original samples after EXIF 6.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0185_png_source_numeric libpng none 8 rgb 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-oriented-8-rgb.png" orientation 0 \
    2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested loader or CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - libpng none 8-bit rgb original samples after EXIF 6"
    exit 0
}
case "${trace}" in *"LSXLOAD1|event=ok|loader=libpng|code=L_OK"*) true ;; *) false ;; esac || {
    echo "not ok 1 - requested PNG loader did not deliver the frame"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply"*) false ;; *) true ;; esac || {
    echo "not ok 1 - disabled CMS unexpectedly applied"
    exit 0
}
echo "ok 1 - libpng none 8-bit rgb original samples after EXIF 6"
exit 0
