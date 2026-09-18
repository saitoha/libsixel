#!/bin/sh
# Policy: docs/loader/png-color-metadata.md
# Verify libpng builtin 16-bit rgba final gamma_float target.
set -eux
echo "1..1"
set -v

result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0185_png_source_numeric libpng builtin 16 rgba 0 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-p3-background-16-rgba.png" file 0 1 \
    2>&1) || result=$?
test "$result" -ne 77 || {
    echo "ok 1 # SKIP requested loader or CMS engine is unavailable"
    exit 0
}
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - libpng builtin 16-bit rgba final gamma_float target"
    exit 0
}
case "${trace}" in *"LSXLOAD1|event=ok|loader=libpng|code=L_OK"*) true ;; *) false ;; esac || {
    echo "not ok 1 - requested PNG loader did not deliver the frame"
    exit 0
}
case "${trace}" in *"LSXCMS2|event=apply|engine=builtin|path=extended-rgb|src=4|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "not ok 1 - requested CMS did not apply to original float samples"
    exit 0
}
echo "ok 1 - libpng builtin 16-bit rgba final gamma_float target"
exit 0
