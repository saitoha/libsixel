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
test "${value#*'"evaluator": "builtin"'}" != "$value" || {
    echo "not ok 1 - missing color context field"
    exit 0
}
test "${value#*'"requested_loader_cms": "lcms2"'}" != "$value" || {
    echo "not ok 1 - missing color context field"
    exit 0
}
test "${value#*'"plain_sixel_default": "srgb"'}" != "$value" || {
    echo "not ok 1 - missing color context field"
    exit 0
}
test "${value#*'"region_count": 4'}" != "$value" || {
    echo "not ok 1 - missing color context field"
    exit 0
}
test "${value#*'"pcs": "XYZ D50, Y=1"'}" != "$value" || {
    echo "not ok 1 - missing color context field"
    exit 0
}
test "${value#*'"requested_definition": "display-p3"'}" != "$value" || {
    echo "not ok 1 - missing color context field"
    exit 0
}
test "${value#*'"background_definition": "srgb"'}" != "$value" || {
    echo "not ok 1 - missing color context field"
    exit 0
}
test "${value#*'"signal_precision": "float32"'}" != "$value" || {
    echo "not ok 1 - missing color context field"
    exit 0
}
test "${value#*'"current_format": "LINEARRGBFLOAT32"'}" != "$value" || {
    echo "not ok 1 - missing actual float frame format"
    exit 0
}
echo "ok 1 - color context json"
exit 0
