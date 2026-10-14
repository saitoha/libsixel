#!/bin/sh
# Policy: docs/quality/metrics.md
set -eux
echo "1..1"
set -v

value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp" "${TOP_SRCDIR}/tests/data/inputs/snake_16.bmp") || {
    echo "not ok 1 - scielab default JSON"
    exit 0
}
case "${value}" in
    *'"scielab_ppd": 60'*'"S-CIELAB": 0.000000'*'"S-CIELAB_max": 0.000000'*) ;;
    *) echo "not ok 1 - scielab JSON contract"; exit 0 ;;
esac

echo "ok 1 - scielab default JSON metrics and viewing geometry"
exit 0
