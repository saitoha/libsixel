#!/bin/sh
# Policy: docs/quality/color-evaluation.md
# Verify color context json.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -R srgb -T display-p3 -w 8 -P float32 -# lcms2 "${TOP_SRCDIR}/tests/data/color-numeric/red.six" "${TOP_SRCDIR}/tests/data/color-numeric/red.six") || {
    echo "not ok 1 - JSON evaluation failed"
    exit 0
}
case "${value}" in *'"evaluator": "builtin"'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing color context field"
    exit 0
}
case "${value}" in *'"requested_loader_cms": "lcms2"'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing color context field"
    exit 0
}
case "${value}" in *'"plain_sixel_default": "srgb"'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing color context field"
    exit 0
}
case "${value}" in *'"region_count": 4'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing color context field"
    exit 0
}
case "${value}" in *'"pcs": "XYZ D50, Y=1"'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing color context field"
    exit 0
}
case "${value}" in *'"requested_definition": "display-p3"'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing color context field"
    exit 0
}
case "${value}" in *'"background_definition": "srgb"'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing color context field"
    exit 0
}
case "${value}" in *'"signal_precision": "float32"'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing color context field"
    exit 0
}
case "${value}" in *'"current_format": "LINEARRGBFLOAT32"'*) true ;; *) false ;; esac || {
    echo "not ok 1 - missing actual float frame format"
    exit 0
}
echo "ok 1 - color context json"
exit 0
