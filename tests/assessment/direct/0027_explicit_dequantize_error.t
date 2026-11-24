#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Reject explicit dequantize.
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -D -Tdisplay-p3 -dk_undither "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six" 2>&1) || status=$?
test "$status" -eq 2 || {
    printf '\n%s\n' "not ok 1 - invalid combination was not rejected"
    exit 0
}
case "${value}" in *'explicit target colorspace with dequantize'*) true ;; *) false ;; esac || {
    printf '\n%s\n' "not ok 1 - rejection reason missing"
    exit 0
}
printf '\n%s\n' "ok 1 - explicit dequantize error"
exit 0
