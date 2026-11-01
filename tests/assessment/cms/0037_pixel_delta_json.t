#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify peak diagnostics and tails in the full quality report.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -R srgb -T srgb "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - JSON evaluation failed"
    exit 0
}
case "${value}" in *'"DE00_max": 0.000000'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing maximum metric"
    exit 0
}
case "${value}" in *'"maximum_xy": [0, 0]'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing peak location"
    exit 0
}
case "${value}" in *'"strictly_above": {"1": 0, "5": 0, "10": 0, "20": 0, "50": 0}'*) true ;; *) false ;; esac || {
    echo "not ok 1 - incorrect tail counts"
    exit 0
}
echo "ok 1 - pixel delta JSON"
exit 0
