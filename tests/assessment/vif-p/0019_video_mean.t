#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P participates in video mean selection and quality bounds.
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -Ma --reference-fps=1 -m VIF-P -b VIF-P:min=1 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.six" \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.six") || {
    echo "not ok 1 - VIF-P video mean"
    exit 0
}
test "${value}" = 1.000000 || {
    echo "not ok 1 - VIF-P video identity"
    exit 0
}
echo "ok 1 - VIF-P video mean selection and bounds"
exit 0
