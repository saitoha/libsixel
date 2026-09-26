#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify managed gray mixed default with independent values or qualified failures.
set -eux
export SIXEL_LOADER_CMS_COLORSYNC_NATIVE_GAMMA=0
result=0
trace=$(SIXEL_TRACE_TOPIC=loader,loader_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0274_managed_gray_mixed_default \
    "${TOP_SRCDIR}/tests/data/color-numeric/tiff-managed-mixed.tif" 2>&1) || result=$?
test "$result" -ne 77 || {
    echo "1..0 # SKIP libtiff or requested CMS is unavailable"
    set -v
    exit 0
}
echo "1..1"
set -v
test "$result" -eq 0 || {
    echo "$trace" >&2
    echo "not ok 1 - managed gray mixed default"
    exit 0
}
case "${trace}" in *"TMVALUE|reference=1|case-begin"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - reference control absent"
    exit 0
}
observed=${trace#*"TMVALUE|reference=1|case-begin"}
case "${observed}" in *"LSXCMS2|event=apply|engine=colorsync|"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - requested engine did not apply"
    exit 0
}
case "${observed}" in *"|ok=1"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected native apply result"
    exit 0
}
case "${observed}" in *"to=para3"*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - mixed device domain preparation differs"
    exit 0
}
remaining=${observed#*'LSXCMS2|event=apply|engine=colorsync|'}
case "${observed}" in *'LSXCMS2|event=apply|engine=colorsync|'*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - expected native group did not apply"
    exit 0
}
observed=$remaining
remaining=${observed#*'LSXCMS2|event=apply|engine=colorsync|'}
case "${observed}" in *'LSXCMS2|event=apply|engine=colorsync|'*) true ;; *) false ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - expected native group did not apply"
    exit 0
}
observed=$remaining
case "${observed}" in *'LSXCMS2|event=apply|'*) false ;; *) true ;; esac || {
    echo "$trace" >&2
    echo "not ok 1 - unexpected additional CMS apply"
    exit 0
}
echo "ok 1 - managed gray mixed default"
exit 0
