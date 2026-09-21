#!/bin/sh
# Policy: docs/loader/libpng.md
# Preserve indexed gamut through an actual ColorSync transform.
set -eux
result=0
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/libpng_indexed_precision 15 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP required loader or CMS engine unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - Preserve indexed gamut through an actual ColorSync transform"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|engine=colorsync|"*"src=4|dst=4|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - required colorsync transform did not apply"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|engine=builtin|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS engine fallback"
    exit 0
}
case "$trace" in *"LSXCMS2|event=apply|engine=lcms2|"*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected CMS engine fallback"
    exit 0
}
echo "ok 1 - Preserve indexed gamut through an actual ColorSync transform"
exit 0
